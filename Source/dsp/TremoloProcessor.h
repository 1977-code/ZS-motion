#pragma once

#include "ModeProcessor.h"
#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Tremolo — amplitude modulation.

        gain(t) = 1 − depth + depth · normalisedLFO(t)

    so at depth 0 the gain is flat and at depth 1 it dips to silence at the LFO
    trough. Left and right use the stereo-offset LFO the engine already prepared.

    A short one-pole slew on the gain (≈1.5 ms) rounds the corners of the square
    and saw shapes so they modulate without clicking, which is the cheap,
    robust alternative to poly-BLEP for a control-rate signal.
*/
class TremoloProcessor final : public ModeProcessor
{
public:
    void prepare (double newSampleRate, int /*maxBlockSize*/) override
    {
        sampleRate = newSampleRate;
        slewCoeff  = 1.0f - std::exp (-1.0f / (float) (0.0015 * sampleRate));  // ~1.5 ms
        reset();
    }

    void reset() override
    {
        gainL = gainR = 1.0f;
        depthSmoothed.prepare (sampleRate, 0.02f, depthSmoothed.getTarget());
    }

    void setDepth (float depth01) noexcept override
    {
        depthSmoothed.setTarget (math::clamp (depth01, 0.0f, 1.0f));
    }

    void processSample (float inL, float inR,
                        float lfoL, float lfoR,
                        float& outL, float& outR) noexcept override
    {
        const float depth = depthSmoothed.getNextValue();

        const float targetL = 1.0f - depth + depth * (lfoL * 0.5f + 0.5f);
        const float targetR = 1.0f - depth + depth * (lfoR * 0.5f + 0.5f);

        gainL += slewCoeff * (targetL - gainL);
        gainR += slewCoeff * (targetR - gainR);

        outL = inL * gainL;
        outR = inR * gainR;
    }

private:
    double sampleRate = 44100.0;
    float  slewCoeff  = 0.05f;
    float  gainL = 1.0f, gainR = 1.0f;
    SmoothedParameter depthSmoothed;
};

} // namespace zs
