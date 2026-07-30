#pragma once

#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"
#include <array>

namespace zs::fan
{

/**
    The blade ring modulator, built to the measurements rather than to a guess.

    What the reference plug-in actually does, read off its output with the bench in
    Tests/Compare.cpp (a 1 kHz tone in, spectrum out):

      • the tone itself is **gone** — 99 dB down. Only multiplication by a
        zero-mean carrier does that, so this is genuine ring modulation and not the
        amplitude modulation the sibling plug-in uses;
      • the sidebands form a comb whose spacing equals the Rate exactly. At 2 Hz
        every even offset rings and every odd one is silent; at 1 Hz the comb steps
        in ones. So the carrier is the LFO itself, not a separate oscillator;
      • at 1 Hz the comb has nulls on the 5th, 10th and 15th harmonics, which is
        the fingerprint of a rectangular pulse one fifth of a period wide;
      • with Depth at zero the output is silent, so Depth scales the carrier
        rather than blending toward dry.

    All of which is one shape: a rectangle open for a fifth of the turn, shifted to
    zero mean so the carrier cancels. Physically it reads as a blade sweeping past —
    a short bright pass, a longer shaded rest — and the sign flip across the two is
    exactly what makes it ring rather than throb.

    A first attempt used a plain rectangle rounded off with a one-pole filter. It
    got the nulls right but not the balance: a rectangle's harmonics fall away as
    1/k, so the comb sloped down from the first, while the measured one is nearly
    flat across the first nineteen and then falls off a cliff.

    Flat with nulls every fifth is the signature of a **pair of opposite impulses**
    a fifth of a cycle apart, not a plateau — the blade's two edges rather than its
    face. Their spectrum is |sin(πkd)|, which is flat in k and nulls exactly at
    k = 5, 10, 15 for d = 1/5, and the pair being equal and opposite is what makes
    the carrier sum to nothing. The cliff is then simply where the band-limiting
    stops, at the nineteenth.

    So the carrier is built additively, once, into a wavetable:

        table(t) = Σ  sin(πkd) · sin(2πkt)        k = 1 … 19,  d = 1/5
        out      = in · depth · table(phase)

    Band-limited by construction, so it cannot alias however fast it spins, and
    zero-mean by construction, so the carrier cancels.
*/
class RingFan
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        buildTable();

        depthSmoothed.prepare (sampleRate, 0.02f, depthSmoothed.getTarget());
        updateIncrement();
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0f;
    }

    void setRateHz (float hz) noexcept
    {
        rateHz = math::clamp (hz, 0.0f, maxRateHz);
        updateIncrement();
    }

    void setDepth (float depth01) noexcept
    {
        depthSmoothed.setTarget (math::clamp (depth01, 0.0f, 1.0f));
    }

    /** Offsets the right channel's blade, so the effect is not dead centre. */
    void setStereoOffset (float turns) noexcept { stereoOffset = math::wrap01 (turns); }

    void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float depth = depthSmoothed.getNextValue();

        phase += increment;
        if (phase >= 1.0f)
            phase -= std::floor (phase);

        outL = inL * depth * carrierAt (phase);
        outR = inR * depth * carrierAt (math::wrap01 (phase + stereoOffset));
    }

    /** The carrier shape itself, computed rather than looked up.

        Public and static so the interface can draw the very same curve the audio
        thread runs, instead of a stand-in that only looks similar. Costs a handful
        of sines per point, which is nothing for drawing and is why the audio path
        uses the table instead. */
    static float shapeAt (float t) noexcept
    {
        // Peak found once, on first use, so drawing and audio share a scale.
        static const float peak = peakOfShape();

        const float raw = rawShape (t);
        return peak > 0.0f ? raw / peak : raw;
    }

private:
    /** The impulse pair, as a sum of harmonics: flat across the band, nulls every
        fifth, and nothing above the nineteenth — which is what was measured. */
    static float rawShape (float t) noexcept
    {
        float sum = 0.0f;

        for (int k = 1; k <= harmonics; ++k)
            sum += std::sin (math::pi * (float) k * duty)
                 * std::sin (math::twoPi * (float) k * t);

        return sum;
    }

    static float peakOfShape() noexcept
    {
        float peak = 0.0f;

        for (int i = 0; i < tableSize; ++i)
            peak = std::max (peak, std::abs (rawShape ((float) i / (float) tableSize)));

        return peak;
    }

    /** Linear read of the band-limited table — it is smooth, so this is plenty. */
    float carrierAt (float p) const noexcept
    {
        const float x  = p * (float) tableSize;
        const int   i0 = (int) x;
        const float f  = x - (float) i0;

        const float a = table[(size_t) (i0 % tableSize)];
        const float b = table[(size_t) ((i0 + 1) % tableSize)];

        return math::lerp (a, b, f);
    }

    /** Lay the carrier out once, at prepare time, through the shared shape. */
    void buildTable()
    {
        for (int i = 0; i < tableSize; ++i)
            table[(size_t) i] = shapeAt ((float) i / (float) tableSize);
    }

    void updateIncrement() noexcept { increment = (float) (rateHz / sampleRate); }

    /** One fifth of a turn — from the nulls on the 5th, 10th and 15th harmonics. */
    static constexpr float duty      = 0.2f;
    /** Where the measured comb falls off its cliff. */
    static constexpr int   harmonics = 19;
    static constexpr int   tableSize = 4096;
    static constexpr float maxRateHz = 32.0f;   // measured range of the reference

    double sampleRate = 44100.0;
    float  rateHz     = 1.0f;
    float  increment  = 0.0f;
    float  phase      = 0.0f;
    float  stereoOffset = 0.12f;

    std::array<float, tableSize> table {};

    SmoothedParameter depthSmoothed;
};

} // namespace zs::fan
