#pragma once

// Oversampler
//
// Halfband polyphase, 2x per stage, cascaded for 4x. Lives in the core rather
// than using juce::dsp::Oversampling so that LimiterCore stays free of JUCE and
// the offline renderer hears exactly what the plugin hears - including the
// aliasing at 1x, which is a deliberate part of the character.
//
// Filter: 49-tap halfband, Kaiser window beta 7.0.
//   stopband  -72 dB
//   passband  flat to 0.2 of the base rate
//
// A halfband has every even tap at zero except the centre, so one branch is a
// pure delay and only 24 multiplies per stage are actually performed.

namespace notsure
{

class HalfbandStage
{
public:
    static constexpr int kNumOdd = 24;
    static constexpr int kDelay  = kNumOdd / 2;   // 12 base-rate samples

    void reset() noexcept;

    // One base-rate sample in, two samples out at twice the rate.
    void up (float in, float& outEven, float& outOdd) noexcept;

    // Two samples in at twice the rate, one base-rate sample out.
    float down (float inEven, float inOdd) noexcept;

private:
    static const float kCoeffs[kNumOdd];

    // Circular histories: one write per call and a masked read, instead of
    // shifting the whole array every sample. Size is the next power of two above
    // the longest look-back (24), so the wrap is a bitmask, not a modulo. The
    // FIR is also folded on the coefficients' symmetry (h[i] == h[N-1-i]), which
    // halves the multiplies to 12 per stage.
    static constexpr int kRing = 32;
    static constexpr int kMask = kRing - 1;

    float upBuf   [kRing] {};
    float downEven[kRing] {};
    float downOdd [kRing] {};
    int   upPos   = 0;
    int   downPos = 0;
};

class Oversampler
{
public:
    // factor must be 1, 2 or 4. Anything else is clamped to 1.
    void setFactor (int newFactor) noexcept;
    int  getFactor() const noexcept { return factor; }

    // Round-trip group delay in base-rate samples. Report this to the host and
    // use it to delay the dry path, or Mix will comb-filter.
    int getLatencySamples() const noexcept;

    void reset() noexcept;

    // Runs one base-rate sample up, through the shaper at the oversampled
    // rate, and back down. Templated so the shaper inlines - a std::function
    // here would cost more than the filtering.
    template <typename Shaper>
    float process (float in, Shaper&& shape) noexcept
    {
        if (factor == 1)
            return shape (in);

        float a, b;
        stage1.up (in, a, b);

        if (factor == 2)
            return stage1.down (shape (a), shape (b));

        float aa, ab, ba, bb;
        stage2.up (a, aa, ab);
        stage2.up (b, ba, bb);

        const float da = stage2.down (shape (aa), shape (ab));
        const float db = stage2.down (shape (ba), shape (bb));

        return stage1.down (da, db);
    }

private:
    HalfbandStage stage1, stage2;
    int factor = 1;
};

} // namespace notsure
