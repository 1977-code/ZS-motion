#pragma once

#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Equal-power Dry/Wet blend.

        dryGain = cos(mix · π/2)      wetGain = sin(mix · π/2)

    A plain linear crossfade dips in level through the middle when the two sides
    are uncorrelated (which modulation makes them); equal-power holds the
    loudness steady across the sweep. The wet path here reports no added latency,
    so the dry needs no compensating delay.
*/
class DryWetMixer
{
public:
    void prepare (double sampleRate) noexcept
    {
        mixSmoothed.prepare (sampleRate, 0.02f, mixSmoothed.getTarget());
    }

    void reset() noexcept {}

    void setMix (float mix01) noexcept { mixSmoothed.setTarget (math::clamp (mix01, 0.0f, 1.0f)); }

    void processSample (float dryL, float dryR, float wetL, float wetR,
                        float& outL, float& outR) noexcept
    {
        const float mix = mixSmoothed.getNextValue();

        float dryGain, wetGain;
        math::equalPowerGains (mix, dryGain, wetGain);

        outL = dryGain * dryL + wetGain * wetL;
        outR = dryGain * dryR + wetGain * wetR;
    }

private:
    SmoothedParameter mixSmoothed;
};

} // namespace zs
