#include "PluginProcessor.h"
#include "PluginEditor.h"

NotSureProcessor::NotSureProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    crushParam    = apvts.getRawParameterValue (ParamID::crush);
    crunchParam   = apvts.getRawParameterValue (ParamID::crunch);
    sagParam      = apvts.getRawParameterValue (ParamID::sag);
    darknessParam = apvts.getRawParameterValue (ParamID::darkness);
    autoGainParam = apvts.getRawParameterValue (ParamID::autoGain);
    mixParam      = apvts.getRawParameterValue (ParamID::mix);
    trimParam     = apvts.getRawParameterValue (ParamID::trim);
    attackParam   = apvts.getRawParameterValue (ParamID::attackMode);
    bypassParam   = apvts.getRawParameterValue (ParamID::bypass);
    qualityParam  = apvts.getRawParameterValue (ParamID::oversampling);
}

void NotSureProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;

    core.prepare (sampleRate);

    for (auto& ch : bypassDelay)
        ch.fill (0.0f);
    bypassWritePos = 0;

    // Fixed latency, reported once and never changed: the core pads every factor
    // up to the same total. A moving latency would make the host re-align the
    // track and could shift an offline bounce by a few samples.
    setLatencySamples (core.getLatencySamples());
}

void NotSureProcessor::releaseResources()
{
    core.reset();
}

bool NotSureProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void NotSureProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // Flushes denormals to zero for the duration of this block. Without it the
    // long Sag release decays into subnormal floats and CPU load explodes on
    // what is effectively silence.
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Bypass. The signal still has to come out delayed by exactly the latency
    // we report (36), or the host's latency compensation shifts the bypassed
    // audio against everything else - a bounce would not line up against a
    // bypassed reference. So pass through a matching delay rather than returning
    // the buffer untouched. The same parameter is handed to the host via
    // getBypassParameter(), so its bypass button lands here too.
    if (bypassParam->load() > 0.5f)
    {
        const int latency = getLatencySamples();
        const int numCh   = juce::jmin (totalOut, 2);
        const int n       = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const int readPos = (bypassWritePos - latency + kBypassRing) & (kBypassRing - 1);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                bypassDelay[(size_t) ch][(size_t) bypassWritePos] = d[i];
                d[i] = bypassDelay[(size_t) ch][(size_t) readPos];
            }
            bypassWritePos = (bypassWritePos + 1) & (kBypassRing - 1);
        }
        return;
    }

    // --- DSP chain -----------------------------------------------------------
    // Stage 1: feedback limiter with charge-dependent release.
    // Stage 2: asymmetric waveshaper (Crunch), oversampled.
    // Stage 3: Darkness tilt and auto gain. Chain complete.
    // ------------------------------------------------------------------------

    static constexpr float attackTimes[] { 0.3f, 1.3f, 4.0f };
    const auto attackIndex = juce::jlimit (0, 2, static_cast<int> (attackParam->load()));

    static constexpr int qualityFactors[] { 1, 2, 4 };
    const auto qualityIndex = juce::jlimit (0, 2, static_cast<int> (qualityParam->load()));

    notsure::LimiterCore::Params p;
    p.crush    = crushParam->load();
    p.crunch   = crunchParam->load();
    p.darkness = darknessParam->load();
    p.autoGain = autoGainParam->load() > 0.5f;
    p.sag      = sagParam->load();
    p.quality  = qualityFactors[qualityIndex];
    p.attackMs = attackTimes[attackIndex];
    p.mix      = mixParam->load();
    p.trimDb   = trimParam->load();

    // Read per block: hosts flip this when a bounce starts and ends. Offline,
    // the core runs the highest factor the sample-rate cap allows.
    p.offline  = isNonRealtime();

    core.setParams (p);

    auto* left  = buffer.getWritePointer (0);
    auto* right = totalOut > 1 ? buffer.getWritePointer (1) : left;

    core.process (left, right, buffer.getNumSamples());

    // Latency is fixed (reported once in prepareToPlay); nothing to update here.
}

juce::AudioProcessorEditor* NotSureProcessor::createEditor()
{
    return new NotSureEditor (*this);
}

juce::AudioProcessorParameter* NotSureProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamID::bypass);
}

void NotSureProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NotSureProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NotSureProcessor();
}
