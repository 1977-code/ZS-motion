#pragma once

#include "ModeProcessor.h"
#include "FractionalDelay.h"
#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    A compact rotary-speaker ("Leslie") model.

    The mono sum of the input is split into two bands at ~800 Hz — a fast horn on
    top, a slower drum below — and each band is spun independently. Every rotor
    combines the three things a rotating source actually does:

      • Doppler   — a sinusoidally modulated delay, so pitch rises as the mouth
                    swings toward the listener and falls as it swings away;
      • Amplitude — louder when it faces front (tied to cos of the angle);
      • Panning   — constant-power L/R placement tied to sin of the angle.

    Horn and drum keep their own phases at different speeds, so the two never line
    up — the shimmer that makes a Leslie sound alive. Crucially they also have
    *inertia*: when the Rate changes the rotors do not jump, they spin up quickly
    and coast down slowly (the drum heavier than the horn), which is the whole
    character of switching a Leslie between slow and fast. Depth scales pan width,
    AM and Doppler together; Rate sets the target horn speed.
*/
class RotaryProcessor final : public ModeProcessor
{
public:
    void prepare (double newSampleRate, int /*maxBlockSize*/) override
    {
        sampleRate = newSampleRate;
        crossCoeff = 1.0f - std::exp (-math::twoPi * crossoverHz / (float) sampleRate);

        const int maxDelaySamples = (int) (8.0 * 0.001 * sampleRate) + 4;
        hornDelay.prepare (maxDelaySamples);
        drumDelay.prepare (maxDelaySamples);

        hornAccel = coeff (hornAccelSec);
        hornDecel = coeff (hornDecelSec);
        drumAccel = coeff (drumAccelSec);
        drumDecel = coeff (drumDecelSec);

        reset();
    }

    void reset() override
    {
        hornDelay.reset();
        drumDelay.reset();
        lpState   = 0.0f;
        hornPhase = 0.0f;
        drumPhase = 0.25f;                     // start out of alignment
        hornSpeed = hornTarget;                // no spin-up on load, only on change
        drumSpeed = drumTarget;
        depthSmoothed.prepare (sampleRate, 0.03f, depthSmoothed.getTarget());
    }

    void setDepth (float depth01) noexcept override
    {
        depthSmoothed.setTarget (math::clamp (depth01, 0.0f, 1.0f));
    }

    void setRateHz (float hz) noexcept override
    {
        const float r = math::clamp (hz, 0.0f, 12.0f);
        hornTarget = r;
        drumTarget = r * drumRatio;
    }

    /** Rotor positions in [0, 1) and their live speeds — for the interface. */
    float getHornPhase() const noexcept { return hornPhase; }
    float getDrumPhase() const noexcept { return drumPhase; }
    float getHornSpeed() const noexcept { return hornSpeed; }

    void processSample (float inL, float inR,
                        float /*lfoL*/, float /*lfoR*/,
                        float& outL, float& outR) noexcept override
    {
        const float depth = depthSmoothed.getNextValue();
        const float mono  = 0.5f * (inL + inR);

        // Split into drum (low) and horn (high).
        lpState += crossCoeff * (mono - lpState);
        const float low  = lpState;
        const float high = mono - low;

        // Inertia: glide the rotor speeds toward their targets.
        glide (hornSpeed, hornTarget, hornAccel, hornDecel);
        glide (drumSpeed, drumTarget, drumAccel, drumDecel);

        advance (hornPhase, hornSpeed / (float) sampleRate);
        advance (drumPhase, drumSpeed / (float) sampleRate);

        float hL, hR, dL, dR;
        spin (hornDelay, high, hornPhase, hornBaseMs, hornDopplerMs, hornAmDepth, depth, hL, hR);
        spin (drumDelay, low,  drumPhase, drumBaseMs, drumDopplerMs, drumAmDepth, depth, dL, dR);

        outL = hL + dL;
        outR = hR + dR;
    }

private:
    static void advance (float& phase, float inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0f) phase -= std::floor (phase);
    }

    static void glide (float& speed, float target, float accel, float decel) noexcept
    {
        speed += (target > speed ? accel : decel) * (target - speed);
    }

    void spin (FractionalDelay& line, float in, float phase,
               float baseMs, float dopplerMs, float amDepthMax, float depth,
               float& oL, float& oR) noexcept
    {
        const float angle = math::twoPi * phase;
        const float s = std::sin (angle);
        const float c = std::cos (angle);

        const float delayMs = baseMs + dopplerMs * depth * s;
        const float wet = line.processSample (in, delayMs * 0.001f * (float) sampleRate);

        const float amp = 1.0f - amDepthMax * depth + amDepthMax * depth * (0.5f + 0.5f * c);

        // Constant-power pan; depth narrows the swing toward centre.
        const float pan    = s * depth;
        const float panAng = (pan * 0.5f + 0.5f) * math::halfPi;

        oL = wet * amp * std::cos (panAng);
        oR = wet * amp * std::sin (panAng);
    }

    float coeff (float seconds) const noexcept
    {
        if (seconds <= 0.0f) return 1.0f;
        return 1.0f - std::exp (-1.0f / (float) (seconds * sampleRate));
    }

    static constexpr float crossoverHz   = 800.0f;
    static constexpr float drumRatio     = 0.75f;
    static constexpr float hornBaseMs    = 2.0f;
    static constexpr float hornDopplerMs = 1.6f;
    static constexpr float drumBaseMs    = 3.0f;
    static constexpr float drumDopplerMs = 0.8f;
    static constexpr float hornAmDepth   = 0.6f;
    static constexpr float drumAmDepth   = 0.35f;

    // Inertia time constants (seconds): horn spins up briskly, drum lumbers.
    static constexpr float hornAccelSec = 0.7f;
    static constexpr float hornDecelSec = 1.2f;
    static constexpr float drumAccelSec = 2.0f;
    static constexpr float drumDecelSec = 4.0f;

    double sampleRate = 44100.0;
    float  crossCoeff = 0.1f;
    float  lpState    = 0.0f;

    float hornPhase = 0.0f, drumPhase = 0.25f;
    float hornSpeed = 1.0f, drumSpeed = 0.75f;
    float hornTarget = 1.0f, drumTarget = 0.75f;
    float hornAccel = 0.001f, hornDecel = 0.001f;
    float drumAccel = 0.001f, drumDecel = 0.001f;

    FractionalDelay hornDelay, drumDelay;
    SmoothedParameter depthSmoothed;
};

} // namespace zs
