#include "Oversampler.h"

#include <algorithm>
#include <iterator>

namespace notsure
{

// 49-tap halfband, Kaiser beta 7.0, normalised to unity DC gain.
// Only the odd taps are stored - the even ones are zero apart from the centre,
// which is exactly 0.5 and is handled by the pure-delay branch.
const float HalfbandStage::kCoeffs[HalfbandStage::kNumOdd] =
{
    -0.0001870809f, +0.0006043800f, -0.0014282280f, +0.0028705963f,
    -0.0052039846f, +0.0087866842f, -0.0141300538f, +0.0220786045f,
    -0.0343297133f, +0.0552363532f, -0.1008538707f, +0.3165420809f,
    +0.3165420809f, -0.1008538707f, +0.0552363532f, -0.0343297133f,
    +0.0220786045f, -0.0141300538f, +0.0087866842f, -0.0052039846f,
    +0.0028705963f, -0.0014282280f, +0.0006043800f, -0.0001870809f
};

void HalfbandStage::reset() noexcept
{
    std::fill (std::begin (upBuf),    std::end (upBuf),    0.0f);
    std::fill (std::begin (downEven), std::end (downEven), 0.0f);
    std::fill (std::begin (downOdd),  std::end (downOdd),  0.0f);
    upPos = downPos = 0;
}

void HalfbandStage::up (float in, float& outEven, float& outOdd) noexcept
{
    // Zero-stuffing halves the level, so the interpolation filter carries a
    // gain of 2. The centre tap is 0.5, hence the delay branch passes through
    // at unity.
    upBuf[upPos] = in;

    outEven = upBuf[(upPos - kDelay + kRing) & kMask];

    // Symmetric FIR: pair tap i with tap (kNumOdd-1-i), which share a
    // coefficient, so 12 multiplies cover all 24 taps.
    float sum = 0.0f;
    for (int i = 0; i < kNumOdd / 2; ++i)
    {
        const float a = upBuf[(upPos - i + kRing)                 & kMask];
        const float b = upBuf[(upPos - (kNumOdd - 1 - i) + kRing) & kMask];
        sum += kCoeffs[i] * (a + b);
    }
    outOdd = 2.0f * sum;

    upPos = (upPos + 1) & kMask;
}

float HalfbandStage::down (float inEven, float inOdd) noexcept
{
    // Decimation uses the filter at unity DC gain, so the delay branch keeps
    // the centre tap's 0.5 and the odd branch is unscaled.
    downEven[downPos] = inEven;
    downOdd[downPos]  = inOdd;

    float sum = 0.5f * downEven[(downPos - kDelay + kRing) & kMask];

    // The odd branch sits one sample later than the even branch, hence the
    // (i+1) look-back. Folded on symmetry the same way as the up filter: tap
    // (i+1) pairs with tap (kNumOdd-i).
    for (int i = 0; i < kNumOdd / 2; ++i)
    {
        const float a = downOdd[(downPos - (i + 1) + kRing)     & kMask];
        const float b = downOdd[(downPos - (kNumOdd - i) + kRing) & kMask];
        sum += kCoeffs[i] * (a + b);
    }

    downPos = (downPos + 1) & kMask;
    return sum;
}

void Oversampler::setFactor (int newFactor) noexcept
{
    const int wanted = (newFactor == 2 || newFactor == 4) ? newFactor : 1;

    if (wanted != factor)
    {
        factor = wanted;
        reset();
    }
}

void Oversampler::reset() noexcept
{
    stage1.reset();
    stage2.reset();
}

int Oversampler::getLatencySamples() const noexcept
{
    // Each halfband contributes kDelay samples on the way up and kDelay on the
    // way down, measured at the rate it runs. The second stage runs at twice
    // the base rate, so its contribution is halved when referred back.
    if (factor == 1)
        return 0;

    if (factor == 2)
        return 2 * HalfbandStage::kDelay;

    return 2 * HalfbandStage::kDelay + HalfbandStage::kDelay;
}

} // namespace notsure
