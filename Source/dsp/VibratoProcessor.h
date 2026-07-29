#pragma once

#include "ModeProcessor.h"
#include "FractionalDelay.h"
#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Vibrato — a pure modulated delay, no dry inside the algorithm. Sweeping the
    read pointer moves the pitch up and down; that is the whole effect.

    The base delay is kept comfortably above the interpolation minimum so the
    read pointer can never cross the write pointer even at full depth (the spec's
    "avoid zero delay" note). With the global Dry/Wet below 100 % this naturally
    becomes a chorus, which is the documented behaviour of the original.
*/
class VibratoProcessor final : public ModeProcessor
{
public:
    void prepare (double newSampleRate, int /*maxBlockSize*/) override
    {
        sampleRate = newSampleRate;

        const int maxDelaySamples = (int) (maxTotalDelayMs * 0.001 * sampleRate) + 4;
        delayL.prepare (maxDelaySamples);
        delayR.prepare (maxDelaySamples);

        reset();
    }

    void reset() override
    {
        delayL.reset();
        delayR.reset();
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
        const float modMs  = depth * maxModMs;

        outL = delayL.processSample (inL, msToSamples (baseMs + modMs * lfoL));
        outR = delayR.processSample (inR, msToSamples (baseMs + modMs * lfoR));
    }

private:
    float msToSamples (float ms) const noexcept { return ms * 0.001f * (float) sampleRate; }

    static constexpr float baseMs          = 8.0f;
    static constexpr float maxModMs        = 6.0f;   // base > maxMod ⇒ delay stays > 0
    static constexpr float maxTotalDelayMs = 16.0f;

    double sampleRate = 44100.0;
    FractionalDelay delayL, delayR;
    SmoothedParameter depthSmoothed;
};

} // namespace zs
