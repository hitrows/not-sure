#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// Parameter IDs.
//
// These strings are written into every saved session and every preset file.
// Once the plugin is in use, changing a string here silently breaks recall for
// everyone. Treat them as permanent.
// ---------------------------------------------------------------------------
namespace ParamID
{
    inline constexpr auto crush        = "crush";
    inline constexpr auto crunch       = "crunch";
    inline constexpr auto sag          = "sag";
    inline constexpr auto darkness     = "darkness";
    inline constexpr auto mix          = "mix";
    inline constexpr auto trim         = "trim";
    inline constexpr auto autoGain     = "autogain";
    inline constexpr auto attackMode   = "attack";
    inline constexpr auto oversampling = "oversampling";
    inline constexpr auto bypass       = "bypass";
}

// The version hint. AU uses it to keep parameter identity stable across
// plugin updates. Bump it only when adding parameters, never when editing one.
inline constexpr int kParamVersion = 1;

namespace ParamDetail
{
    // 0..10 dial, like the hardware. Skew pushes resolution toward the low end
    // where the useful range lives - the top of Crush is all the same kind of
    // destruction, the bottom is where the character changes fast.
    inline juce::NormalisableRange<float> dialRange (float skew = 1.0f)
    {
        juce::NormalisableRange<float> r { 0.0f, 10.0f, 0.01f };
        r.setSkewForCentre (skew);
        return r;
    }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using PID   = juce::ParameterID;
    using Attrs = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- main dials ---------------------------------------------------------
    //
    // Everything starts at zero, so a freshly inserted instance is transparent
    // and any change the user hears is one they made. Note that this means an
    // untouched instance does nothing at all - the factory presets are what
    // demonstrate the character.

    layout.add (std::make_unique<APF> (
        PID { ParamID::crush, kParamVersion }, "Crush",
        ParamDetail::dialRange (4.0f), 0.0f));

    layout.add (std::make_unique<APF> (
        PID { ParamID::crunch, kParamVersion }, "Crunch",
        ParamDetail::dialRange (4.0f), 0.0f));

    // Sag: 0 = ordinary limiter release, 10 = the 20-second tar pit.
    layout.add (std::make_unique<APF> (
        PID { ParamID::sag, kParamVersion }, "Sag",
        ParamDetail::dialRange (5.0f), 0.0f));

    layout.add (std::make_unique<APF> (
        PID { ParamID::darkness, kParamVersion }, "Darkness",
        ParamDetail::dialRange (5.0f), 0.0f));

    layout.add (std::make_unique<APF> (
        PID { ParamID::mix, kParamVersion }, "Mix",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
        Attrs{}.withLabel ("%")));

    // --- output -------------------------------------------------------------

    layout.add (std::make_unique<APF> (
        PID { ParamID::trim, kParamVersion }, "Trim",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        Attrs{}.withLabel ("dB")));

    layout.add (std::make_unique<APB> (
        PID { ParamID::autoGain, kParamVersion }, "Auto gain", true));

    // The original is fixed at 1.3 ms. Slow attack is the whole reason
    // transients survive - this exposes it rather than hiding it.
    layout.add (std::make_unique<APC> (
        PID { ParamID::attackMode, kParamVersion }, "Attack",
        juce::StringArray { "0.3 ms", "1.3 ms", "4.0 ms" }, 1));

    // --- quality ------------------------------------------------------------
    //
    // There is no stereo mode. The hardware was mono, and a single linked
    // detector across both channels keeps the image stable. If mid/side ever
    // becomes worth having it gets added as a new parameter - never by
    // reviving this ID, since that would change what old sessions recall.

    // Applied around the waveshaper only, not the whole chain.
    layout.add (std::make_unique<APC> (
        PID { ParamID::oversampling, kParamVersion }, "Quality",
        juce::StringArray { "1x (dirty)", "2x", "4x (clean)" }, 2));

    // Bypass. Registered as a real parameter and handed to the host through
    // getBypassParameter(), so the host's own bypass button and our lever are
    // one switch, not two that fight each other.
    layout.add (std::make_unique<APB> (
        PID { ParamID::bypass, kParamVersion }, "Bypass", false));

    return layout;
}
