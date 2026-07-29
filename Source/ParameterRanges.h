#pragma once

#include <cmath>
#include <algorithm>

/**
    Plain numbers and tempo-sync maths shared by the DSP, the parameter layout
    and the tests. Deliberately free of any JUCE dependency.
*/
namespace zs::params
{
    // ── LFO rate (manual) ────────────────────────────────────────────────────
    inline constexpr float rateMinHz     = 0.01f;
    inline constexpr float rateMaxHz     = 10.0f;
    inline constexpr float rateDefaultHz = 1.0f;

    // ── Fan carrier rate ─────────────────────────────────────────────────────
    inline constexpr float fanRateMinHz     = 2.0f;
    inline constexpr float fanRateMaxHz     = 40.0f;
    inline constexpr float fanRateDefaultHz = 6.0f;

    inline constexpr float outputMinDb = -24.0f;
    inline constexpr float outputMaxDb =  12.0f;

    // ── Tempo sync ───────────────────────────────────────────────────────────
    // Division index → length in quarter notes (1/4 note == 1 quarter).
    inline float divisionInQuarters (int divisionIndex) noexcept
    {
        static const float quarters[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f }; // 1/1 … 1/32
        const int n = (int) (sizeof (quarters) / sizeof (quarters[0]));
        return quarters[std::clamp (divisionIndex, 0, n - 1)];
    }

    // Modifier index → period multiplier (straight, dotted ×1.5, triplet ×2/3).
    inline float modifierMultiplier (int modifierIndex) noexcept
    {
        switch (modifierIndex)
        {
            case 1:  return 1.5f;        // dotted
            case 2:  return 2.0f / 3.0f; // triplet
            default: return 1.0f;        // straight
        }
    }

    /** Effective LFO frequency (Hz) for a tempo-synced division at a given BPM. */
    inline float syncedFrequency (double bpm, int divisionIndex, int modifierIndex) noexcept
    {
        if (bpm <= 0.0)
            return rateDefaultHz;

        const double quarterSeconds = 60.0 / bpm;
        const double period = quarterSeconds
                            * (double) divisionInQuarters (divisionIndex)
                            * (double) modifierMultiplier (modifierIndex);

        if (period <= 0.0)
            return rateDefaultHz;

        return (float) (1.0 / period);
    }
}
