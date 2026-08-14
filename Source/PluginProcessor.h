#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/LimiterCore.h"

class NotSureProcessor final : public juce::AudioProcessor
{
public:
    NotSureProcessor();
    ~NotSureProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    // Hands the host the same bypass parameter our UI drives, so the host's
    // bypass button and our lever are one switch.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }

    // Sag can hold the envelope open for ~20 s. Telling the host the tail is
    // long stops it truncating renders mid-decay.
    double getTailLengthSeconds() const override           { return 25.0; }

    // No AU factory presets: presets ship as .aupreset files in category
    // folders under ~/Library/Audio/Presets, which Logic shows in its Settings
    // menu. Exposing factory programs here would add a redundant flat
    // "AU Presets" list, so we deliberately report none.
    int getNumPrograms() override                          { return 0; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // The oversampling factor actually running (may differ from the Quality
    // choice after the sample-rate cap / offline boost). For the Quality tooltip.
    int getEffectiveOversampling() const noexcept { return core.getEffectiveFactor(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    // Cached raw pointers. Reading these is a plain atomic load; looking a
    // parameter up by string ID inside processBlock would not be.
    std::atomic<float>* crushParam    = nullptr;
    std::atomic<float>* crunchParam   = nullptr;
    std::atomic<float>* sagParam      = nullptr;
    std::atomic<float>* darknessParam = nullptr;
    std::atomic<float>* mixParam      = nullptr;
    std::atomic<float>* trimParam     = nullptr;
    std::atomic<float>* attackParam   = nullptr;
    std::atomic<float>* qualityParam  = nullptr;
    std::atomic<float>* autoGainParam = nullptr;
    std::atomic<float>* bypassParam   = nullptr;

    // All character lives here. The processor only marshals parameters.
    notsure::LimiterCore core;

    // Bypass delay so the bypassed signal carries the same latency we report.
    // Power of two > the 36-sample reported latency, so the wrap is a mask.
    static constexpr int kBypassRing = 64;
    std::array<std::array<float, kBypassRing>, 2> bypassDelay {};
    int bypassWritePos = 0;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotSureProcessor)
};
