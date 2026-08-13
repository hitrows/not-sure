#pragma once

#include "Oversampler.h"

// LimiterCore
//
// The character of the plugin lives here. Deliberately free of any JUCE
// include so it can be compiled into a plain console program and run against
// wav files - tuning by ear against files is far faster than reloading a DAW.
//
// Stage 1: feedback detector, gain computer, attack, charge-dependent release.
// Stage 2: asymmetric waveshaper (Crunch) with oversampling around it only.
// Stage 3: Darkness tilt and auto gain.
//
// The detector is always linked across both channels. The hardware was mono,
// and a single detector keeps the stereo image stable under heavy reduction.

namespace notsure
{

class LimiterCore
{
public:
    // Defaults match Parameters.h: everything at zero, so an untouched
    // instance is transparent.
    struct Params
    {
        float crush    = 0.0f;    // 0..10, drive into the limiter
        float crunch   = 0.0f;    // 0..10, drive into the output stage
        float sag      = 0.0f;    // 0..10, how far charge stretches release
        float darkness = 0.0f;    // 0..10, post-distortion tilt
        float attackMs = 1.3f;    // 0.3 / 1.3 / 4.0
        float mix      = 100.0f;  // 0..100 %
        float trimDb   = 0.0f;    // -24..+24
        int   quality  = 4;       // oversampling factor: 1, 2 or 4
        bool  autoGain = true;    // compensate loudness for crush and crunch

        // Defaulted so setParams can skip the coefficient recompute when a block
        // brings no change. Written as = default to keep the memberwise float
        // compare out of -Wfloat-equal.
        friend bool operator== (const Params&, const Params&) = default;
    };

    void prepare (double sampleRate);
    void reset();

    // Cheap: stores values and recomputes only what changed.
    // Safe to call once per block.
    void setParams (const Params& newParams);

    // In-place. Pass the same pointer twice for mono.
    void process (float* left, float* right, int numSamples);

    // Base-rate samples introduced by the oversampler. Report this to the host
    // and expect it to change when quality changes.
    int getLatencySamples() const noexcept { return oversamplerL.getLatencySamples(); }

    // For a future meter. Negative dB, 0 means no reduction.
    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    void updateCoefficients();
    void updateReleaseCoefficient();

    // --- fixed design constants ---------------------------------------------

    // The threshold does not move. Crush drives into it - that is the whole
    // interface idea, inherited from the hardware's fixed-threshold design.
    static constexpr float kThresholdDb   = -30.0f;

    // Drive and makeup are locked together: makeup is exactly minus the drive
    // floor, so Crush 0 passes at unity. The floor also has to sit far enough
    // below the threshold that full-scale material is not limited at zero -
    // an earlier version used -30 dB, which put a 0 dBFS peak exactly on the
    // threshold and clipped peaks with every knob down.
    static constexpr float kDriveFloorDb  = -42.0f;
    static constexpr float kDriveSpanDb   =   5.2f;   // reaches +10 dB at 10

    // Depth of reduction, in dB, that counts as "fully charged".
    static constexpr float kFullChargeDb  =  30.0f;

    // Release at Sag = 0, and the multiplier reached at Sag = 10 fully charged.
    // 0.2 s * 100 = 20 s, matching the hardware's saturated release.
    static constexpr float kMinReleaseSec   = 0.2f;
    static constexpr float kMaxReleaseRatio = 100.0f;

    // How fast the charge itself moves. Rising is quick; draining is slow,
    // which is what makes release shorten gradually after a dense passage.
    static constexpr float kChargeAttackMs  = 50.0f;
    static constexpr float kChargeDecaySec  = 2.0f;

    // Crunch spans this much extra drive into the output stage, and this much
    // DC offset. The offset generates even harmonics - fat rather than fuzzy -
    // and is removed again by the DC blocker afterwards.
    static constexpr float kCrunchMinShape  = 0.2f;   // effectively linear
    static constexpr float kCrunchMaxShape  = 6.0f;   // heavy saturation
    static constexpr float kCrunchMaxOffset = 0.35f;
    static constexpr float kDcBlockerHz     = 10.0f;

    // Darkness is a tilt, not a plain lowpass. A lowpass at 1.5 kHz removes
    // energy; a tilt moves it, so the sound gets darker without getting
    // smaller. The low-shelf lift also compensates the low-frequency thinning
    // that this class of circuit produces as a side effect.
    static constexpr float kTiltPivotHz    = 900.0f;
    static constexpr float kTiltMaxCutDb   = 14.0f;
    static constexpr float kTiltMaxBoostDb =  4.0f;

    // Keeps the detector out of subnormal territory during silence. With a
    // 20-second release the envelope tail would otherwise decay into
    // denormals and CPU load would explode on nothing.
    static constexpr float kDenormalGuard = 1.0e-15f;

    // The release coefficient depends on charge, and recomputing an exp per
    // sample is exactly the kind of cost we said we would not pay. Charge
    // moves on a multi-second scale, so refreshing it every 32 samples
    // (~0.7 ms at 44.1 kHz) is far finer than the signal needs.
    static constexpr int kControlRateDivider = 32;

    // Comfortably larger than the longest oversampler latency (36).
    static constexpr int kMaxLatency = 64;

    // Gains that multiply the signal ramp toward their targets over this time,
    // so moving a knob or loading a preset does not click.
    static constexpr float kSmoothingSec = 0.02f;

    // --- state --------------------------------------------------------------

    double sampleRate = 44100.0;

    Params params;
    bool   coefficientsDirty = true;

    float driveGain      = 1.0f;
    float makeupGain     = 1.0f;
    float trimGain       = 1.0f;
    float wetAmount      = 1.0f;
    float sagAmount      = 0.0f;   // 0..1
    float crunchDrive    = 1.0f;
    float crunchOffset   = 0.0f;
    float tiltCoeff      = 0.0f;
    float tiltLowGain    = 1.0f;
    float tiltHighGain   = 1.0f;
    float autoGainLin    = 1.0f;

    // Smoothed copies of the gains that multiply the signal directly. The values
    // above are the per-block targets; these follow them one pole at a time.
    // Time constants (attack/release/tilt pole) and the constant makeup gain do
    // not need smoothing, so they are not duplicated here.
    float smoothCoeff     = 0.0f;
    bool  smoothingPrimed = false;
    float smDrive         = 1.0f;
    float smCrunchDrive   = 1.0f;
    float smCrunchOffset  = 0.0f;
    float smTiltLow       = 1.0f;
    float smTiltHigh      = 1.0f;
    float smAutoGain      = 1.0f;
    float smWet           = 1.0f;
    float smTrim          = 1.0f;

    float attackCoeff        = 0.0f;
    float releaseCoeff       = 0.0f;
    float chargeAttackCoeff  = 0.0f;
    float chargeDecayCoeff   = 0.0f;
    float dcBlockerCoeff     = 0.0f;

    float gainReductionDb = 0.0f;  // <= 0
    float currentGain     = 1.0f;  // fed back into the next sample
    float charge          = 0.0f;  // 0..1

    Oversampler oversamplerL, oversamplerR;

    // Tilt filter state, one per channel.
    float tiltLpL = 0.0f, tiltLpR = 0.0f;

    // DC blocker state, one per channel.
    float dcX1L = 0.0f, dcY1L = 0.0f;
    float dcX1R = 0.0f, dcY1R = 0.0f;

    // The dry path must be delayed to match the oversampler, or Mix below
    // 100% comb-filters the top end.
    float dryDelayL[kMaxLatency] {};
    float dryDelayR[kMaxLatency] {};
    int   dryWritePos = 0;
};

} // namespace notsure
