#pragma once

#include "ModeProcessor.h"
#include "FractionalDelay.h"
#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Two-voice stereo chorus.

    Each channel runs two modulated taps at slightly different base delays; the
    left and right LFOs are already stereo-offset by the engine, so the image
    opens up on its own. A gentle, HF-damped feedback path adds the familiar
    flanger-leaning resonance without turning metallic.

    Output is wet only — the global Dry/Wet adds the dry back. At the default
    ~50 % wet this is a classic chorus; pushed to 100 % it thins toward a doubled
    vibrato, which is the expected behaviour.
*/
class ChorusProcessor final : public ModeProcessor
{
public:
    void prepare (double newSampleRate, int /*maxBlockSize*/) override
    {
        sampleRate = newSampleRate;

        const int maxDelaySamples = (int) (maxTotalDelayMs * 0.001 * sampleRate) + 4;
        for (auto* d : { &delayL, &delayR })
            d->prepare (maxDelaySamples);

        dampCoeff = onePole (2500.0f);
        reset();
    }

    void reset() override
    {
        delayL.reset();
        delayR.reset();
        fbL = fbR = 0.0f;
        dampL = dampR = 0.0f;
        depthSmoothed.prepare (sampleRate, 0.02f, depthSmoothed.getTarget());
    }

    void setDepth (float depth01) noexcept override
    {
        depthSmoothed.setTarget (math::clamp (depth01, 0.0f, 1.0f));
    }

    void setFeedback (float fb) noexcept { feedback = math::clamp (fb, -0.95f, 0.95f); }

    void processSample (float inL, float inR,
                        float lfoL, float lfoR,
                        float& outL, float& outR) noexcept override
    {
        const float depth  = depthSmoothed.getNextValue();
        const float modMs   = minModMs + depth * (maxModMs - minModMs);

        outL = renderChannel (delayL, inL, lfoL, modMs, fbL, dampL);
        outR = renderChannel (delayR, inR, lfoR, modMs, fbR, dampR);
    }

private:
    float renderChannel (FractionalDelay& line, float in, float lfo, float modMs,
                         float& fbState, float& dampState) noexcept
    {
        // Feed input plus the damped feedback of the previous wet sample.
        line.push (in + fbState);

        const float d1 = baseMs1 + modMs * lfo;
        const float d2 = baseMs2 + modMs * (-lfo);   // second voice moves the other way

        const float v1 = line.readHermite (msToSamples (d1));
        const float v2 = line.readHermite (msToSamples (d2));
        const float wet = 0.5f * (v1 + v2);

        // One-pole HF damping in the feedback so repeats darken instead of ringing.
        dampState += dampCoeff * (wet - dampState);
        fbState = feedback * dampState;

        return wet;
    }

    float msToSamples (float ms) const noexcept { return ms * 0.001f * (float) sampleRate; }

    float onePole (float hz) const noexcept
    {
        const float x = std::exp (-math::twoPi * hz / (float) sampleRate);
        return 1.0f - x;
    }

    static constexpr float baseMs1        = 11.0f;
    static constexpr float baseMs2        = 15.0f;
    static constexpr float minModMs       = 1.0f;
    static constexpr float maxModMs       = 7.0f;
    static constexpr float maxTotalDelayMs = 30.0f;   // baseMs2 + maxModMs + head-room

    double sampleRate = 44100.0;
    float  feedback   = 0.0f;
    float  dampCoeff  = 0.2f;

    FractionalDelay delayL, delayR;
    float fbL = 0.0f, fbR = 0.0f;
    float dampL = 0.0f, dampR = 0.0f;

    SmoothedParameter depthSmoothed;
};

} // namespace zs
