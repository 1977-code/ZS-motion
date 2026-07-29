#pragma once

#include "MathUtils.h"

namespace zs
{

/**
    A minimal linear-ramp parameter smoother — the JUCE-free equivalent of
    juce::SmoothedValue used across the DSP so the modulation core carries no
    module dependency and can be unit-tested on its own.

    setTarget() moves toward the new value over the ramp time set in prepare();
    getNextValue() advances one sample. Cheap, branch-light, allocation-free.
*/
class SmoothedParameter
{
public:
    void prepare (double sampleRate, float rampSeconds, float initialValue = 0.0f) noexcept
    {
        const double n = std::max (1.0, sampleRate * (double) std::max (0.0f, rampSeconds));
        stepCount   = (int) n;
        current     = initialValue;
        target      = initialValue;
        increment   = 0.0f;
        countdown   = 0;
    }

    void setImmediate (float value) noexcept
    {
        current = target = value;
        increment = 0.0f;
        countdown = 0;
    }

    void setTarget (float newTarget) noexcept
    {
        target = newTarget;

        const float diff = target - current;

        if (stepCount <= 1 || std::abs (diff) < 1.0e-12f)
        {
            current   = target;
            countdown = 0;
            return;
        }

        increment = diff / (float) stepCount;
        countdown = stepCount;
    }

    float getNextValue() noexcept
    {
        if (countdown <= 0)
            return current;

        current += increment;

        if (--countdown == 0)
            current = target;

        return current;
    }

    /** Advance n samples at once and return the resulting value (block-rate use). */
    float skip (int n) noexcept
    {
        if (n <= 0 || countdown <= 0)
            return current;

        if (n >= countdown)
        {
            current   = target;
            countdown = 0;
            return current;
        }

        current   += increment * (float) n;
        countdown -= n;
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget()  const noexcept { return target; }
    bool  isSmoothing() const noexcept { return countdown > 0; }

private:
    float current   = 0.0f;
    float target    = 0.0f;
    float increment = 0.0f;
    int   stepCount = 1;
    int   countdown = 0;
};

} // namespace zs
