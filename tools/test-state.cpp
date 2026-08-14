// Feeds NotSureProcessor::setStateInformation the cases STATE-VERSION-SPEC.md
// part 1 asks to check for real: an empty block, random bytes, a truncated
// state, XML from a different plugin, a future schema, a pre-0.8.4 state (no
// stateSchema attribute), and an out-of-range value. Prints PASS/FAIL per
// case and exits non-zero if anything fails.
//
// Recompiles the same sources as the plugin target (see CMakeLists.txt),
// borrowing ${PROJECT_NAME}'s COMPILE_DEFINITIONS property so every
// JucePlugin_* macro it needs is exactly the one the real plugin was built
// with - no hand-picked, easy-to-drift subset.

#include "../Source/PluginProcessor.h"
#include "../Source/Licence.h"

#include <iostream>
#include <random>
#include <vector>

namespace
{
    int failures = 0;

    void check (const juce::String& label, bool ok)
    {
        std::cout << (ok ? "PASS  " : "FAIL  ") << label << "\n";
        if (! ok)
            ++failures;
    }

    // Pushes every parameter to the opposite end of its range from its
    // default, so a load that silently does nothing (rather than genuinely
    // applying or genuinely rejecting the state) shows up as a failure
    // instead of looking like success. 0.0/1.0 normalised are always legal -
    // they are a range's own endpoints - which matters for the choice and
    // bool parameters: an arbitrary value like 0.75 is not a value any real
    // host would ever send them, and setStateInformation's own clamp would
    // correctly snap it on load, which would look like a bug in the load
    // path when the actual mismatch was here in the test.
    void detune (NotSureProcessor& p)
    {
        for (auto* param : p.getParameters())
            param->setValueNotifyingHost (param->getDefaultValue() < 0.5f ? 1.0f : 0.0f);
    }

    bool allParamsInRange (NotSureProcessor& p)
    {
        for (auto* param : p.getParameters())
        {
            const float v = param->getValue();
            if (v < 0.0f || v > 1.0f)
                return false;
        }
        return true;
    }

    bool sameParams (NotSureProcessor& a, NotSureProcessor& b)
    {
        for (int i = 0; i < a.getParameters().size(); ++i)
            if (! juce::approximatelyEqual (a.getParameters()[i]->getValue(),
                                             b.getParameters()[i]->getValue()))
                return false;
        return true;
    }
}

int main()
{
    // AudioProcessorValueTreeState privately inherits juce::Timer (it flushes
    // parameter values to its ValueTree at 10 Hz) and Timer needs a running
    // MessageManager to exist even if nothing ever pumps its message loop -
    // without this, construction still works but asserts loudly. A real host
    // always brings one up before touching the processor; this is the console
    // equivalent.
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Informational, not a check: constructing the first processor triggers
    // LicenceChecker's one-shot read (LICENCE-SPEC.md), against whatever is
    // actually at ~/Library/Application Support/Hitrows/Not Sure/licence.txt
    // on this machine right now, verified with the real embedded public key.
    // Prints the same status the version-screw tooltip would show - a way to
    // confirm the embedded key actually matches a real issued licence without
    // needing to hover over the plugin in a host.
    std::cout << "licence status: " << notsure::LicenceChecker::getInstance().getStatusText() << "\n\n";

    // 1: empty block.
    {
        NotSureProcessor p;
        p.prepareToPlay (44100.0, 512);
        detune (p);
        p.setStateInformation (nullptr, 0);
        check ("empty block: no crash, stays in range", allParamsInRange (p));
    }

    // 2: random bytes.
    {
        NotSureProcessor p;
        p.prepareToPlay (44100.0, 512);
        detune (p);
        std::mt19937 rng (12345);
        std::vector<char> junk (512);
        for (auto& b : junk)
            b = (char) (rng() & 0xff);
        p.setStateInformation (junk.data(), (int) junk.size());
        check ("random bytes: no crash, stays in range", allParamsInRange (p));
    }

    // 3: a real, valid save truncated in half.
    {
        NotSureProcessor source;
        source.prepareToPlay (44100.0, 512);
        detune (source);
        juce::MemoryBlock full;
        source.getStateInformation (full);

        NotSureProcessor p;
        p.prepareToPlay (44100.0, 512);
        detune (p);
        p.setStateInformation (full.getData(), (int) full.getSize() / 2);
        check ("truncated state: no crash, stays in range", allParamsInRange (p));
    }

    // 4: valid XML, but from a different plugin (foreign root tag and IDs).
    {
        NotSureProcessor p;
        p.prepareToPlay (44100.0, 512);
        detune (p);

        juce::XmlElement foreign ("SomeOtherPluginState");
        auto* param = new juce::XmlElement ("PARAM");
        param->setAttribute ("id", "gain");
        param->setAttribute ("value", 99.0);
        foreign.addChildElement (param);

        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (foreign, mb);
        p.setStateInformation (mb.getData(), (int) mb.getSize());
        check ("foreign plugin XML: no crash, stays in range", allParamsInRange (p));
    }

    // 5: round trip at the current schema.
    {
        NotSureProcessor source;
        source.prepareToPlay (44100.0, 512);
        detune (source);
        juce::MemoryBlock saved;
        source.getStateInformation (saved);

        NotSureProcessor dest;
        dest.prepareToPlay (44100.0, 512);
        dest.setStateInformation (saved.getData(), (int) saved.getSize());

        check ("round trip at current schema: every parameter recalls exactly",
               sameParams (source, dest));
    }

    // 6: what 0.8.3 and earlier actually wrote - no stateSchema attribute at
    // all. Strip it from an otherwise-real save and confirm it still loads.
    {
        NotSureProcessor source;
        source.prepareToPlay (44100.0, 512);
        detune (source);
        juce::MemoryBlock saved;
        source.getStateInformation (saved);

        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        if (xml == nullptr) { check ("pre-0.8.4 state (no stateSchema): still loads intact", false); }
        else
        {
            xml->removeAttribute ("stateSchema");
            xml->removeAttribute ("pluginVersion");
            juce::MemoryBlock stripped;
            juce::AudioProcessor::copyXmlToBinary (*xml, stripped);

            NotSureProcessor dest;
            dest.prepareToPlay (44100.0, 512);
            dest.setStateInformation (stripped.getData(), (int) stripped.getSize());

            check ("pre-0.8.4 state (no stateSchema): still loads intact", sameParams (source, dest));
        }
    }

    // 7: a state from a schema newer than this build understands, with real
    // values. Must load the values - not refuse, not wipe to defaults.
    {
        NotSureProcessor source;
        source.prepareToPlay (44100.0, 512);
        detune (source);
        juce::MemoryBlock saved;
        source.getStateInformation (saved);

        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        if (xml == nullptr) { check ("stateSchema=99 (future): loads values, no crash, nothing wiped", false); }
        else
        {
            xml->setAttribute ("stateSchema", 99);
            juce::MemoryBlock future;
            juce::AudioProcessor::copyXmlToBinary (*xml, future);

            NotSureProcessor dest;
            dest.prepareToPlay (44100.0, 512);
            dest.setStateInformation (future.getData(), (int) future.getSize());

            check ("stateSchema=99 (future): loads values, no crash, nothing wiped",
                   sameParams (source, dest));
        }
    }

    // 8: a value outside the parameter's declared range must come back
    // clamped, not passed through.
    {
        NotSureProcessor p;
        p.prepareToPlay (44100.0, 512);

        juce::XmlElement xml ("PARAMETERS");
        xml.setAttribute ("stateSchema", 1);
        auto* param = new juce::XmlElement ("PARAM");
        param->setAttribute ("id", "crush"); // range 0-10
        param->setAttribute ("value", 500.0);
        xml.addChildElement (param);

        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (xml, mb);
        p.setStateInformation (mb.getData(), (int) mb.getSize());

        auto* crush = p.apvts.getParameter ("crush");
        const float denormalised = crush->convertFrom0to1 (crush->getValue());
        check ("out-of-range value: clamped into range (got " + juce::String (denormalised) + ")",
               denormalised <= 10.0f + 1.0e-3f);
    }

    std::cout << "\n" << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
