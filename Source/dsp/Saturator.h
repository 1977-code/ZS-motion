#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>

#include "../utils/MathUtils.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    Three flavours of waveshaping, driven by one 0..1 Saturation control, with
    optional oversampling to keep the aliasing hard drive produces under control.

      Soft   — symmetric tanh, odd harmonics, peak-normalised;
      Asym   — different curvature per half → even harmonics; a DC blocker
               cleans up the offset that creates;
      Hard   — clip, for the aggressive end.

    This runs as a block stage on the wet buffer (before the modulation loop), so
    the whole non-linearity can be lifted to 2× or 4× with a minimal-latency IIR
    polyphase oversampler, shaped up there, and brought back down — the classic,
    correct way to tame the harmonics that fold back as aliasing. Off / 2× / 4×
    is chosen with setQuality(); the IIR polyphase filters are treated as
    zero-latency, so the dry path needs no compensating delay.

    Amount both raises the drive and blends the shaped signal in, so at 0 the
    stage is bit-transparent and the onset is smooth.
*/
class Saturator
{
public:
    enum class Character { Soft = 0, Asymmetric, Hard };

    void prepare (double newSampleRate, int maxBlockSize, int channels)
    {
        baseSampleRate = newSampleRate;
        numChannels    = juce::jmax (1, channels);
        maxBlock       = juce::jmax (1, maxBlockSize);

        amountSmoothed.prepare (baseSampleRate, 0.02f, amountSmoothed.getTarget());

        // Linear-phase FIR rather than the cheaper polyphase IIR, and integer
        // latency. The IIR's group delay varies with frequency, so no single delay
        // can line the dry path back up against it — mixing the two then combs. FIR
        // costs a little more and delays every frequency by the same whole number of
        // samples, which is what makes the compensation in the engine exact.
        using OS = juce::dsp::Oversampling<float>;
        os2x = std::make_unique<OS> ((size_t) numChannels, 1, OS::filterHalfBandFIREquiripple, true, true);
        os4x = std::make_unique<OS> ((size_t) numChannels, 2, OS::filterHalfBandFIREquiripple, true, true);
        os2x->initProcessing ((size_t) maxBlock);
        os4x->initProcessing ((size_t) maxBlock);

        reset();
    }

    void reset() noexcept
    {
        dcX1[0] = dcX1[1] = dcY1[0] = dcY1[1] = 0.0f;
        if (os2x) os2x->reset();
        if (os4x) os4x->reset();
    }

    void setAmount (float a01)      noexcept { amountSmoothed.setTarget (math::clamp (a01, 0.0f, 1.0f)); }
    void setCharacter (Character c) noexcept { character = c; }
    void setQuality (int q)         noexcept { quality = juce::jlimit (0, 2, q); }

    static constexpr float maxDriveDb = 18.0f;

    /** Drive gain for a 0..1 Saturation setting. */
    static float driveGainFor (float amount01) noexcept
    {
        return math::dbToGain (math::clamp (amount01, 0.0f, 1.0f) * maxDriveDb);
    }

    /** The transfer curve on its own, with no state.

        Shared with the interface so the rotor can be drawn through the very same
        non-linearity the audio goes through — driving the plug-in then visibly
        deforms the sculpture instead of only tinting it. */
    static float curve (float x, float driveGain, Character character) noexcept
    {
        switch (character)
        {
            case Character::Hard:
                return math::clamp (driveGain * x, -1.0f, 1.0f);

            case Character::Asymmetric:
            {
                const float g = x >= 0.0f ? driveGain : driveGain * 0.6f;
                return std::tanh (g * x) / std::tanh (driveGain);
            }

            case Character::Soft:
            default:
                return std::tanh (driveGain * x) / std::tanh (driveGain);
        }
    }

    /** Latency the current quality setting adds, in samples at the base rate.

        The oversampled path is not free: the up/down filters delay it. Anything
        that mixes this stage against an undelayed dry signal has to make up for
        this, or the two comb-filter against each other. */
    float getLatencySamples() const noexcept
    {
        if (quality == 0)
            return 0.0f;

        const auto& os = (quality == 2 ? os4x : os2x);
        return os != nullptr ? (float) os->getLatencyInSamples() : 0.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int channels = juce::jmin (numChannels, buffer.getNumChannels());

        // The oversamplers were sized in prepare(); a host that sends a longer block
        // than it promised would otherwise walk off the end of their buffers.
        const int numSamples = juce::jmin (buffer.getNumSamples(), maxBlock);

        if (channels <= 0 || numSamples <= 0)
            return;

        const float amount = amountSmoothed.skip (numSamples);
        if (amount <= 1.0e-5f)
            return;                                   // untouched == dry

        const float driveGain = math::dbToGain (amount * maxDriveDb);

        // The dry/wet blend of the non-linearity has to happen on signals that sit
        // at the same point in time. Doing it at the base rate against the
        // oversampled output would comb, because the up/down filters delay that
        // output by a few samples — so when we oversample, the blend goes up there
        // with the shaping and comes back down as one signal.
        if (quality == 0)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);

                for (int i = 0; i < numSamples; ++i)
                    d[i] = math::lerp (d[i], shape (d[i], driveGain, ch), amount);
            }

            return;
        }

        auto& os = (quality == 2 ? *os4x : *os2x);

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            (size_t) channels, (size_t) numSamples);
        auto up = os.processSamplesUp (block);

        const int upN = (int) up.getNumSamples();

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = up.getChannelPointer ((size_t) ch);

            for (int i = 0; i < upN; ++i)
                d[i] = math::lerp (d[i], shape (d[i], driveGain, ch), amount);
        }

        os.processSamplesDown (block);
    }

private:
    float shape (float x, float driveGain, int ch) noexcept
    {
        const float y = curve (x, driveGain, character);

        if (character != Character::Asymmetric)
            return y;

        // Remove the DC that the asymmetry introduces.
        const float out = y - dcX1[ch] + 0.9995f * dcY1[ch];
        dcX1[ch] = y;
        dcY1[ch] = out;
        return out;
    }

    Character character = Character::Soft;
    int       quality   = 0;                 // 0 off, 1 = 2x, 2 = 4x
    int       numChannels = 2;
    int       maxBlock  = 512;
    double    baseSampleRate = 44100.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2x, os4x;

    SmoothedParameter amountSmoothed;
    float dcX1[2] = { 0.0f, 0.0f };
    float dcY1[2] = { 0.0f, 0.0f };
};

} // namespace zs
