#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

/**
    Small, JUCE-free numeric helpers shared across the DSP. Kept dependency-free
    so the whole modulation core can be compiled and unit-tested on its own.
*/
namespace zs::math
{
    inline constexpr float pi     = 3.14159265358979323846f;
    inline constexpr float twoPi  = 2.0f * pi;
    inline constexpr float halfPi = 0.5f * pi;

    template <typename T>
    inline T clamp (T v, T lo, T hi) noexcept { return std::min (std::max (v, lo), hi); }

    /** Wrap a phase into [0, 1). */
    inline float wrap01 (float p) noexcept
    {
        p -= std::floor (p);
        return p < 0.0f ? p + 1.0f : p;
    }

    /** Hermite / smoothstep S-curve on [0,1]; flat slope at both ends. */
    inline float smoothstep (float t) noexcept
    {
        t = clamp (t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    inline float dbToGain (float dB) noexcept { return std::pow (10.0f, dB * 0.05f); }
    inline float gainToDb (float g)  noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

    /** Equal-power crossfade gains for a mix control in [0,1]. */
    inline void equalPowerGains (float mix, float& dryGain, float& wetGain) noexcept
    {
        mix     = clamp (mix, 0.0f, 1.0f);
        dryGain = std::cos (mix * halfPi);
        wetGain = std::sin (mix * halfPi);
    }

    /** Tiny, allocation-free RNG (xorshift32) for smooth-random modulation.
        Deterministic per seed, so behaviour is reproducible in tests. */
    class Xorshift
    {
    public:
        explicit Xorshift (uint32_t seed = 0x9e3779b9u) noexcept : state (seed | 1u) {}

        uint32_t nextUint() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        /** Uniform float in [0, 1). */
        float nextFloat() noexcept { return (float) (nextUint() >> 8) * (1.0f / 16777216.0f); }

        /** Uniform float in [-1, 1). */
        float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

    private:
        uint32_t state;
    };
}
