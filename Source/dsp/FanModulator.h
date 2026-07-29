#pragma once

#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    The signature "Fan" effect.

    The reference plug-in calls it a very slow ring modulator; the more
    convincing, more controllable reading — used here — is a blade-windowed
    amplitude modulation. A carrier spins at the Fan rate and each revolution
    sweeps `blades` openings past the signal:

        window(φ)     = shaped(sin(2π · blades · φ))         // Sine / Rounded / Pulse
        modulator(φ)  = (1 − amount) + amount · (swell + blade + resonance + turbulence)
        out           = in · modulator(φ)

    Three carrier shapes trade smoothness for bite. `resonance` mixes in the
    second blade harmonic, sharpening the chop toward a metallic ring;
    `irregularity` unevens the blade spacing and adds filtered turbulence so it
    never sounds like a metronome; a stereo phase offset stops the two channels
    chopping in lockstep. A DC blocker cleans up the one-sided window's offset.
*/
class FanModulator
{
public:
    enum class Shape { Sine = 0, Rounded, Pulse };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        turbCoeff  = 1.0f - std::exp (-math::twoPi * 120.0f / (float) sampleRate); // ~120 Hz LP on noise
        amountSmoothed.prepare (sampleRate, 0.03f, amountSmoothed.getTarget());
        updateIncrement();
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0f;
        turbState = 0.0f;
        dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
    }

    void setRateHz (float hz) noexcept { rateHz = math::clamp (hz, 0.1f, 40.0f); updateIncrement(); }
    void setAmount (float a)  noexcept { amountSmoothed.setTarget (math::clamp (a, 0.0f, 1.0f)); }
    void setBlades (float b)  noexcept { blades = math::clamp (b, 2.0f, 8.0f); }
    void setIrregularity (float i) noexcept { irregularity = math::clamp (i, 0.0f, 1.0f); }
    void setResonance (float r) noexcept { resonance = math::clamp (r, 0.0f, 1.0f); }
    void setShape (int s) noexcept { shape = (Shape) math::clamp (s, 0, 2); }

    /** Carrier position in [0, 1) — for the interface. */
    float getPhase() const noexcept { return phase; }

    void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float amount = amountSmoothed.getNextValue();

        phase += increment;
        if (phase >= 1.0f) phase -= 1.0f;

        // One shared turbulence sample per frame, gently low-passed noise.
        const float noise = rng.nextBipolar();
        turbState += turbCoeff * (noise - turbState);

        const float modL = modulator (phase,                amount);
        const float modR = modulator (phase + stereoOffset, amount);

        outL = dcBlock (inL * modL, dcX1L, dcY1L);
        outR = dcBlock (inR * modR, dcX1R, dcY1R);
    }

private:
    float bladeWindow (float bladeArg) const noexcept
    {
        const float s = std::sin (bladeArg);

        switch (shape)
        {
            case Shape::Rounded: return std::pow (std::max (0.0f, s), 1.5f);
            case Shape::Pulse:   return std::pow (std::max (0.0f, s), 4.0f);
            case Shape::Sine:
            default:             return 0.5f + 0.5f * std::sin (bladeArg - math::halfPi); // raised cosine humps
        }
    }

    float modulator (float ph, float amount) noexcept
    {
        ph = math::wrap01 (ph);

        // Uneven blade spacing when irregularity is up.
        const float warped   = ph + irregularity * 0.25f * std::sin (math::twoPi * ph);
        const float bladeArg = math::twoPi * blades * warped;

        const float blade = bladeWindow (bladeArg);

        // Resonance mixes in the second blade harmonic — sharper, ringier chop.
        const float resTerm = resonance * 0.45f * std::sin (2.0f * bladeArg + 0.3f);

        // A slow once-per-revolution swell under the blades keeps it organic.
        const float swell = 0.15f * (0.5f + 0.5f * std::sin (math::twoPi * ph));

        const float turbulence = irregularity * 0.15f * turbState;

        const float window = math::clamp (swell + 0.85f * blade + resTerm + turbulence, 0.0f, 1.6f);

        return (1.0f - amount) + amount * window;
    }

    float dcBlock (float x, float& x1, float& y1) noexcept
    {
        const float y = x - x1 + 0.9995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    void updateIncrement() noexcept { increment = (float) (rateHz / sampleRate); }

    static constexpr float stereoOffset = 0.15f;   // revolutions between L and R

    double sampleRate   = 44100.0;
    float  rateHz       = 6.0f;
    float  blades       = 4.0f;
    float  irregularity = 0.2f;
    float  resonance    = 0.0f;
    Shape  shape        = Shape::Sine;
    float  increment    = 0.0f;
    float  phase        = 0.0f;

    float  turbCoeff = 0.02f;
    float  turbState = 0.0f;
    math::Xorshift rng { 0xC0FFEEu };

    float dcX1L = 0.0f, dcY1L = 0.0f, dcX1R = 0.0f, dcY1R = 0.0f;

    SmoothedParameter amountSmoothed;
};

} // namespace zs
