#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UpdateChecker.h"

// ---------------------------------------------------------------------------
// Pointer-only look and feel.
//
// The caps in main.png are blank, so this pointer is the only one. The Slider
// is invisible; drawRotarySlider draws nothing but the pointer. Per-knob colour
// (dark on the white crush cap, off-white on the rest) is read from a property
// set on each slider, never sampled from the image.
// ---------------------------------------------------------------------------
class NotSurePointerLNF final : public juce::LookAndFeel_V4
{
public:
    NotSurePointerLNF() = default;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

// ---------------------------------------------------------------------------
// Editor.
//
// Laid out in a fixed 1792 x 592 design space, scaled by one factor computed in
// resized(). The background (main.png) is rendered once into a cached image on
// resize and only blitted in paint(); switches, lamp and bypass lever are child
// components that redraw only themselves. No text is drawn - the artwork carries
// all lettering.
// ---------------------------------------------------------------------------
class NotSureEditor final : public juce::AudioProcessorEditor,
                            private juce::ChangeListener
{
public:
    explicit NotSureEditor (NotSureProcessor&);
    ~NotSureEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    // Update notice: an overlay drawn only when a newer version is known.
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshUpdateNotice();

    static constexpr float designWidth  = 1792.0f;
    static constexpr float designHeight =  592.0f;
    static constexpr int   numKnobs = 6;

    void rebuildBackground();

    NotSureProcessor& processorRef;
    NotSurePointerLNF pointerLnf;
    juce::TooltipWindow tooltip { this };

    // Artwork, straight from the embedded BinaryData - no file is read.
    // main   : powered on, lamp lit, lever up, switches at their baked positions
    // off    : lamp dark + bypass lever down (drawn only when bypassed)
    // overlays: switches at left / centre / right, sparse (see UI-SPEC part 2)
    juce::Image main, off, allLeft, allCentre, allRight;

    // main.png, pre-scaled to the current window size.
    juce::Image cachedBackground;

    std::array<juce::Slider, numKnobs>               knobs;
    std::array<std::unique_ptr<SliderAtt>, numKnobs> knobAtt;
    std::array<juce::Rectangle<int>, numKnobs>       knobCapBox; // design space

    struct Tile
    {
        std::unique_ptr<juce::Component> comp;
        juce::Rectangle<int>             design;
    };
    std::vector<Tile> tiles;

    // "New version" overlay + its click target (design space). Not part of the
    // cached background: it may arrive after resized() has run, so it is drawn
    // in paint() and only its own rectangle is repainted when the result lands.
    juce::Image newver;
    std::unique_ptr<juce::Component> updateLink;

    // Bottom-right screw: hover shows the plugin version. See VersionScrew in
    // PluginEditor.cpp and STATE-VERSION-SPEC.md part 2.
    std::unique_ptr<juce::Component> versionScrew;

    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotSureEditor)
};
