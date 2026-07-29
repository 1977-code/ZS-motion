#pragma once

#include "../utils/MathUtils.h"
#include <cstdint>

namespace zs
{

/** The waveforms every mode's shared LFO can take. Kept in sync with the
    "waveform" parameter choice order. */
enum class Waveform
{
    Sine = 0,
    Triangle,
    SawUp,
    SawDown,
    Square,
    SmoothRandom,
    numWaveforms
};

/**
    The shape of every waveform at phase p in [0, 1), as a bipolar [-1, 1] value.

    Free-standing so the interface can draw exactly the curve the audio thread is
    running — the visualiser is the same maths, not a lookalike. `randA`/`randB`
    are the two smooth-random nodes and are ignored by the other shapes.
*/
inline float waveformShape (Waveform w, float p, float randA = 0.0f, float randB = 0.0f) noexcept
{
    switch (w)
    {
        case Waveform::Sine:         return std::sin (math::twoPi * p);
        case Waveform::Triangle:     return 1.0f - 4.0f * std::abs (p - 0.5f);
        case Waveform::SawUp:        return 2.0f * p - 1.0f;
        case Waveform::SawDown:      return 1.0f - 2.0f * p;
        case Waveform::Square:       return p < 0.5f ? 1.0f : -1.0f;
        case Waveform::SmoothRandom: return math::lerp (randA, randB, math::smoothstep (p));
        case Waveform::numWaveforms:
        default:                     return std::sin (math::twoPi * p);
    }
}

/**
    One phase machine that every LFO-driven mode reads from.

    The whole plug-in shares a single notion of phase so that switching modes,
    changing rate or syncing to the host never makes the movement jump. Phase
    lives in [0, 1); a stereo read is just the same phase sampled at an extra
    offset, which is how the "stereo phase" control decorrelates the channels.

    Usage per sample:
        lfo.advance();
        float l = lfo.value (0.0f);
        float r = lfo.value (stereoOffset01);

    `advance()` and `value()` are separate so both channels read the *same*
    advanced phase and share the smooth-random node state.
*/
class LFO
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset (0.0f);
    }

    void reset (float startPhase01 = 0.0f) noexcept
    {
        phase01     = math::wrap01 (startPhase01);
        randPrev    = rng.nextBipolar();
        randTarget  = rng.nextBipolar();
    }

    void setFrequency (float hz) noexcept
    {
        frequencyHz = hz < 0.0f ? 0.0f : hz;
        increment   = (float) (frequencyHz / sampleRate);
    }

    void setWaveform (Waveform w) noexcept { waveform = w; }

    /** Force the phase (used to lock to the host timeline, drift-free). */
    void setPhase (float p01) noexcept { phase01 = math::wrap01 (p01); }

    float getPhase() const noexcept { return phase01; }
    float getFrequency() const noexcept { return frequencyHz; }

    /** Move the phase on by one sample. On a full wrap, roll a fresh
        smooth-random target so that waveform stays band-limited to the rate. */
    void advance() noexcept
    {
        phase01 += increment;

        if (phase01 >= 1.0f)
        {
            phase01 -= std::floor (phase01);
            randPrev   = randTarget;
            randTarget = rng.nextBipolar();
        }
    }

    /** Bipolar [-1, 1] value of the current waveform at (phase + offset). */
    float value (float phaseOffset01 = 0.0f) const noexcept
    {
        const float p = math::wrap01 (phase01 + phaseOffset01);
        return shape (p);
    }

    /** Unipolar [0, 1] version, for amplitude-style modulation. */
    float unipolar (float phaseOffset01 = 0.0f) const noexcept
    {
        return value (phaseOffset01) * 0.5f + 0.5f;
    }

private:
    float shape (float p) const noexcept
    {
        return waveformShape (waveform, p, randPrev, randTarget);
    }

    double sampleRate  = 44100.0;
    float  frequencyHz = 1.0f;
    float  increment   = 0.0f;
    float  phase01     = 0.0f;

    Waveform waveform  = Waveform::Sine;

    // Smooth-random node state (a new target each cycle, S-curve between them).
    math::Xorshift rng { 0x1234567u };
    float randPrev   = 0.0f;
    float randTarget = 0.0f;
};

} // namespace zs
