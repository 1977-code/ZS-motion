#pragma once

#include "../dsp/LFO.h"
#include "../utils/MathUtils.h"

namespace zs::fan
{

/**
    One LFO with a continuous shape control rather than a list of waveforms.

    The reference plug-in exposes a single knob called Morph instead of a waveform
    selector, so the shape has no steps in it: you sweep from smooth to sharp and
    everything between is a real position, not a snap. This walks a chain of four
    shapes and crossfades between neighbours:

        0.00  sine        round, the classic wobble
        0.33  triangle    even, slightly firmer
        0.67  saw         ramped, one-directional
        1.00  square      hard switch

    The shapes come from the same `zs::waveformShape()` the rest of the line uses,
    so a morphed LFO here and a stepped one in the sibling plug-in agree wherever
    they land on the same shape.
*/
class MorphLFO
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset (float startPhase01 = 0.0f) noexcept
    {
        phase01 = math::wrap01 (startPhase01);
    }

    void setFrequency (float hz) noexcept
    {
        frequencyHz = hz < 0.0f ? 0.0f : hz;
        increment   = (float) (frequencyHz / sampleRate);
    }

    void setMorph (float morph01) noexcept { morph = math::clamp (morph01, 0.0f, 1.0f); }
    void setPhase (float p01)     noexcept { phase01 = math::wrap01 (p01); }

    float getPhase()     const noexcept { return phase01; }
    float getFrequency() const noexcept { return frequencyHz; }

    void advance() noexcept
    {
        phase01 += increment;

        if (phase01 >= 1.0f)
            phase01 -= std::floor (phase01);
    }

    /** Bipolar [-1, 1] at (phase + offset), blended across the shape chain. */
    float value (float phaseOffset01 = 0.0f) const noexcept
    {
        const float p = math::wrap01 (phase01 + phaseOffset01);

        constexpr int count = 4;
        static constexpr Waveform chain[count] { Waveform::Sine, Waveform::Triangle,
                                                Waveform::SawUp, Waveform::Square };

        const float position = morph * (float) (count - 1);
        const int   index    = math::clamp ((int) position, 0, count - 2);
        const float blend    = math::clamp (position - (float) index, 0.0f, 1.0f);

        // Smoothstep the blend so the middle of a morph does not feel like a ramp.
        return math::lerp (waveformShape (chain[index], p),
                           waveformShape (chain[index + 1], p),
                           math::smoothstep (blend));
    }

    float unipolar (float phaseOffset01 = 0.0f) const noexcept
    {
        return value (phaseOffset01) * 0.5f + 0.5f;
    }

private:
    double sampleRate  = 44100.0;
    float  frequencyHz = 1.0f;
    float  increment   = 0.0f;
    float  phase01     = 0.0f;
    float  morph       = 0.0f;
};

} // namespace zs::fan
