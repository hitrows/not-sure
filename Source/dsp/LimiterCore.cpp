#include "LimiterCore.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace notsure
{

namespace
{
    inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }

    inline float gainToDb (float gain) noexcept
    {
        return 20.0f * std::log10 (gain);
    }

    // Fast tanh, a [7/6] Pade approximation clamped so it saturates.
    //
    // The shaper runs at the oversampled rate, so tanh is by far the most-called
    // transcendental in the chain - up to eight std::tanh per output sample at
    // 4x. This matches the real curve to about 1e-4 across the useful range and
    // keeps the small-signal slope at unity (so Crunch 0 stays clean), while
    // costing a handful of multiplies and one divide instead of a library call.
    //
    // The input is clamped to +/-4 because the rational turns back down beyond
    // that; at the clamp it already sits at ~0.9991, so saturation is smooth.
    inline float fastTanh (float x) noexcept
    {
        x = x < -4.0f ? -4.0f : (x > 4.0f ? 4.0f : x);
        const float x2 = x * x;
        const float num = x * (10395.0f + x2 * (1260.0f + x2 * 21.0f));
        const float den = 10395.0f + x2 * (4725.0f + x2 * (210.0f + x2));
        return num / den;
    }

    // Transparent below the knee, bounded at unity above it.
    //
    // Reconstruction after decimation always overshoots: the shaper bounds the
    // signal at the oversampled rate, but the halfband's impulse response has
    // an L1 norm above one, so the base-rate result can exceed what went in.
    // Measured up to +5 dBFS on drum material. This is the base-rate output
    // stage that catches it, and it sits where the hardware's output
    // transformer would.
    inline float softCeiling (float x) noexcept
    {
        constexpr float knee = 0.85f;

        const float a = std::fabs (x);

        if (a <= knee)
            return x;

        const float over = (a - knee) / (1.0f - knee);
        const float bounded = knee + (1.0f - knee) * fastTanh (over);

        return x < 0.0f ? -bounded : bounded;
    }

    // Loudness compensation for Crush and Crunch.
    //
    // The most common complaint about this class of plugin is that every knob
    // changes level, which makes A/B comparison meaningless.
    //
    // Reference point is Crush 0 / Crunch 0, which is unity gain, so auto gain
    // holds the whole range at roughly the level of the untouched signal
    // rather than at some louder setting. An earlier version referenced the
    // parameter defaults instead and consequently boosted by 5 dB with every
    // knob at zero.
    //
    // Fitted to measured output RMS over a crush x crunch grid on drum
    // material. Max residual 0.9 dB. Fitted on one loop, so it is a good
    // approximation rather than a guarantee - re-measure if the gain structure
    // changes, because these numbers are specific to it.
    inline float computeAutoGainDb (float crush, float crunch) noexcept
    {
        // Crush saturates: the limiter clamps, so past a point more drive adds
        // density rather than level. An exponential approach fits this far
        // better than a polynomial, which overshot by 6 dB at the top.
        constexpr float kCrushMax = 31.8f;
        constexpr float kCrushTau =  4.8f;

        // Crunch cuts level, and cuts harder the more Crush is already
        // pushing - the shaper compresses what it is fed. The cut is strongly
        // nonlinear in Crunch itself.
        constexpr float kCutBase  = 2.4f;
        constexpr float kCutSlope = 0.44f;
        constexpr float kCutPower = 2.25f;

        const float crushTerm = kCrushMax * (1.0f - std::exp (-crush / kCrushTau));
        const float cutAmount = (kCutBase + kCutSlope * crushTerm)
                              * std::pow (crunch * 0.1f, kCutPower);

        // Cancel what the chain is predicted to add, referenced to unity at
        // the zero point.
        return -(crushTerm - cutAmount);
    }

    // Standard one-pole coefficient. Returns the per-sample retention factor,
    // so 0 is instant and values near 1 are slow.
    inline float onePoleCoeff (float timeSeconds, double sampleRate) noexcept
    {
        if (timeSeconds <= 0.0f)
            return 0.0f;

        return std::exp (-1.0f / (timeSeconds * static_cast<float> (sampleRate)));
    }
}

void LimiterCore::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    smoothCoeff = onePoleCoeff (kSmoothingSec, sampleRate);
    coefficientsDirty = true;
    reset();
}

void LimiterCore::reset()
{
    // Force a recompute (and smoother prime) on the next block: setParams may
    // now skip when the parameters are unchanged.
    coefficientsDirty = true;

    gainReductionDb = 0.0f;
    currentGain     = 1.0f;
    charge          = 0.0f;

    dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
    tiltLpL = tiltLpR = 0.0f;

    // Snap the smoothers to the first block's targets rather than ramping up
    // from a stale value when playback (re)starts.
    smoothingPrimed = false;

    std::fill (std::begin (dryDelayL), std::end (dryDelayL), 0.0f);
    std::fill (std::begin (dryDelayR), std::end (dryDelayR), 0.0f);
    dryWritePos = 0;

    oversamplerL.reset();
    oversamplerR.reset();
}

void LimiterCore::setParams (const Params& newParams)
{
    // Nothing moved this block - keep the existing coefficients and skip the
    // transcendentals in updateCoefficients. The charge-dependent release is
    // refreshed in the sample loop regardless, so this only elides work that
    // would produce identical values.
    if (newParams == params)
        return;

    params = newParams;
    coefficientsDirty = true;
}

void LimiterCore::updateCoefficients()
{
    // Crush spans 40 dB, from -30 dB to +10 dB.
    //
    // The bottom of the range matters as much as the top. At Crush 0 the drive
    // pushes a full-scale signal down to exactly the threshold, so nothing is
    // limited and the makeup returns it to unity - the plugin is clean. An
    // earlier version started the drive at unity, which left normal material
    // sitting 30 dB over the threshold and crushed hard even at zero.
    driveGain  = dbToGain (kDriveFloorDb + params.crush * kDriveSpanDb);
    makeupGain = dbToGain (-kDriveFloorDb);
    trimGain   = dbToGain (params.trimDb);

    wetAmount = std::clamp (params.mix, 0.0f, 100.0f) * 0.01f;

    sagAmount = std::clamp (params.sag, 0.0f, 10.0f) * 0.1f;

    // The shaper is normalised as tanh(x * d) / d, so d controls curvature
    // rather than level. At d = 0.2 the curve is linear to within about one
    // percent, which is what makes Crunch 0 genuinely clean; an un-normalised
    // tanh at unity drive was clipping hard with every knob at zero.
    const float crunch = std::clamp (params.crunch, 0.0f, 10.0f) * 0.1f;
    crunchDrive  = kCrunchMinShape * std::pow (kCrunchMaxShape / kCrunchMinShape, crunch);
    crunchOffset = crunch * kCrunchMaxOffset;

    attackCoeff = onePoleCoeff (params.attackMs * 0.001f, sampleRate);

    chargeAttackCoeff = onePoleCoeff (kChargeAttackMs * 0.001f, sampleRate);
    chargeDecayCoeff  = onePoleCoeff (kChargeDecaySec, sampleRate);

    dcBlockerCoeff = 1.0f - (2.0f * 3.14159265f * kDcBlockerHz
                             / static_cast<float> (sampleRate));

    const float dark = std::clamp (params.darkness, 0.0f, 10.0f) * 0.1f;
    tiltCoeff    = 1.0f - std::exp (-2.0f * 3.14159265f * kTiltPivotHz
                                    / static_cast<float> (sampleRate));
    tiltLowGain  = dbToGain ( dark * kTiltMaxBoostDb);
    tiltHighGain = dbToGain (-dark * kTiltMaxCutDb);

    autoGainLin = params.autoGain
                    ? dbToGain (computeAutoGainDb (std::clamp (params.crush,  0.0f, 10.0f),
                                                   std::clamp (params.crunch, 0.0f, 10.0f)))
                    : 1.0f;

    oversamplerL.setFactor (params.quality);
    oversamplerR.setFactor (params.quality);

    updateReleaseCoefficient();

    // On the first block after a reset there is nothing to ramp from, so start
    // the smoothers at the targets. After that they follow one pole at a time.
    if (! smoothingPrimed)
    {
        smDrive        = driveGain;
        smCrunchDrive  = crunchDrive;
        smCrunchOffset = crunchOffset;
        smTiltLow      = tiltLowGain;
        smTiltHigh     = tiltHighGain;
        smAutoGain     = autoGainLin;
        smWet          = wetAmount;
        smTrim         = trimGain;
        smoothingPrimed = true;
    }

    coefficientsDirty = false;
}

void LimiterCore::updateReleaseCoefficient()
{
    // This is the mechanism the whole plugin is built around.
    //
    // In the hardware, a capacitor in the detector stores charge proportional
    // to how hard the circuit was driven, and a deeply charged capacitor takes
    // dramatically longer to drain. Release is therefore not a fixed time and
    // not merely program-dependent - it is a function of accumulated history.
    //
    // At Sag = 0 this collapses to a plain 0.2 s release. At Sag = 10 with the
    // detector fully charged it reaches 20 s.
    const float stretch = std::pow (kMaxReleaseRatio, charge * sagAmount);

    releaseCoeff = onePoleCoeff (kMinReleaseSec * stretch, sampleRate);
}

void LimiterCore::process (float* left, float* right, int numSamples)
{
    if (coefficientsDirty)
        updateCoefficients();

    const int latency = oversamplerL.getLatencySamples();

    // The output stage. Asymmetry comes from the DC offset pushed in before
    // the saturation, which biases the curve and generates even harmonics;
    // the DC it leaves behind is removed afterwards.
    //
    // This is also what catches transients. A 1.3 ms attack lets them past the
    // detector, Crush drives them and makeup lifts them, so without a bounded
    // output stage peaks reach absurd levels - measured at +28 dBFS before
    // this existed. In the hardware the output transistor stage does the same
    // job. Crunch is therefore load-bearing, not decoration.
    //
    // Captures the smoothed crunch members by reference so a Crunch move ramps;
    // the oversampler template still inlines it.
    const auto shaper = [this] (float x) noexcept
    {
        return fastTanh (x * smCrunchDrive + smCrunchOffset) / smCrunchDrive;
    };

    int samplesUntilControlUpdate = 0;

    for (int n = 0; n < numSamples; ++n)
    {
        // Refresh the charge-dependent release coefficient at control rate
        // rather than per sample. See kControlRateDivider.
        if (samplesUntilControlUpdate == 0)
        {
            updateReleaseCoefficient();
            samplesUntilControlUpdate = kControlRateDivider;
        }

        --samplesUntilControlUpdate;

        // Ramp the applied gains toward their per-block targets, one pole at a
        // time, so knob moves and preset loads glide instead of stepping.
        smDrive        = driveGain    + (smDrive        - driveGain)    * smoothCoeff;
        smCrunchDrive  = crunchDrive  + (smCrunchDrive  - crunchDrive)  * smoothCoeff;
        smCrunchOffset = crunchOffset + (smCrunchOffset - crunchOffset) * smoothCoeff;
        smTiltLow      = tiltLowGain  + (smTiltLow      - tiltLowGain)  * smoothCoeff;
        smTiltHigh     = tiltHighGain + (smTiltHigh     - tiltHighGain) * smoothCoeff;
        smAutoGain     = autoGainLin  + (smAutoGain     - autoGainLin)  * smoothCoeff;
        smWet          = wetAmount    + (smWet          - wetAmount)    * smoothCoeff;
        smTrim         = trimGain     + (smTrim         - trimGain)     * smoothCoeff;

        const float dryL = left[n];
        const float dryR = right[n];

        const float drivenL = dryL * smDrive;
        const float drivenR = dryR * smDrive;

        // Feedback topology: the gain applied here was computed from the
        // previous sample's output, not from the incoming signal. This is what
        // produces the soft grab and the natural program dependence that a
        // feedforward detector has to imitate.
        const float wetL = drivenL * currentGain;
        const float wetR = drivenR * currentGain;

        // --- detector --------------------------------------------------------

        // The detector reads the fed-back wet signal directly, linked across
        // both channels.
        const float peak = std::max (std::fabs (wetL), std::fabs (wetR)) + kDenormalGuard;
        const float levelDb = gainToDb (peak);
        const float overDb  = levelDb - kThresholdDb;

        // Brick wall: everything above the threshold is removed entirely.
        const float targetGrDb = overDb > 0.0f ? -overDb : 0.0f;

        // A more negative target means we need more reduction, which is the
        // attack direction.
        const float coeff = (targetGrDb < gainReductionDb) ? attackCoeff
                                                           : releaseCoeff;

        gainReductionDb = targetGrDb + (gainReductionDb - targetGrDb) * coeff;

        // --- charge ----------------------------------------------------------

        const float depth = std::clamp (-gainReductionDb / kFullChargeDb, 0.0f, 1.0f);
        const float chargeCoeff = (depth > charge) ? chargeAttackCoeff
                                                   : chargeDecayCoeff;

        charge = depth + (charge - depth) * chargeCoeff;

        currentGain = dbToGain (gainReductionDb);

        // --- output stage, oversampled ---------------------------------------

        float outL = oversamplerL.process (wetL * makeupGain, shaper);
        float outR = oversamplerR.process (wetR * makeupGain, shaper);

        // Remove the DC introduced by the asymmetric offset.
        const float blockedL = outL - dcX1L + dcBlockerCoeff * dcY1L;
        dcX1L = outL; dcY1L = blockedL; outL = blockedL;

        const float blockedR = outR - dcX1R + dcBlockerCoeff * dcY1R;
        dcX1R = outR; dcY1R = blockedR; outR = blockedR;

        outL = softCeiling (outL);
        outR = softCeiling (outR);

        // --- darkness, a first-order tilt --------------------------------------

        tiltLpL += tiltCoeff * (outL - tiltLpL);
        tiltLpR += tiltCoeff * (outR - tiltLpR);

        outL = tiltLpL * smTiltLow + (outL - tiltLpL) * smTiltHigh;
        outR = tiltLpR * smTiltLow + (outR - tiltLpR) * smTiltHigh;

        // Auto gain is the last thing in the wet path, and it has to stay
        // there. Everything before it is nonlinear, so calibration measured
        // with auto gain off only transfers exactly if the compensation is a
        // pure output scaling. Moving it earlier changed how hard the ceiling
        // was hit and threw the calibration out by 7 dB.
        outL *= smAutoGain;
        outR *= smAutoGain;

        // --- dry path, delay-matched ------------------------------------------

        dryDelayL[dryWritePos] = dryL;
        dryDelayR[dryWritePos] = dryR;

        const int readPos = (dryWritePos - latency + kMaxLatency) % kMaxLatency;
        const float alignedDryL = dryDelayL[readPos];
        const float alignedDryR = dryDelayR[readPos];

        dryWritePos = (dryWritePos + 1) % kMaxLatency;

        const float smDry = 1.0f - smWet;
        left[n]  = (outL * smWet + alignedDryL * smDry) * smTrim;
        right[n] = (outR * smWet + alignedDryR * smDry) * smTrim;
    }
}

} // namespace notsure
