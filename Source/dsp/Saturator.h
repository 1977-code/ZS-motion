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

        using OS = juce::dsp::Oversampling<float>;
        os2x = std::make_unique<OS> ((size_t) numChannels, 1, OS::filterHalfBandPolyphaseIIR, true, false);
        os4x = std::make_unique<OS> ((size_t) numChannels, 2, OS::filterHalfBandPolyphaseIIR, true, false);
        os2x->initProcessing ((size_t) maxBlock);
        os4x->initProcessing ((size_t) maxBlock);

        blendScratch.setSize (numChannels, maxBlock);

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

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int channels   = juce::jmin (numChannels, buffer.getNumChannels());
        const int numSamples  = buffer.getNumSamples();
        if (channels <= 0 || numSamples <= 0)
            return;

        const float amount = amountSmoothed.skip (numSamples);
        if (amount <= 1.0e-5f)
            return;                                   // untouched == dry

        const float driveGain = math::dbToGain (amount * maxDriveDb);

        // Keep the pre-saturation signal for the equal-onset blend.
        for (int ch = 0; ch < channels; ++ch)
            blendScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        if (quality == 0)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    d[i] = shape (d[i], driveGain, ch);
            }
        }
        else
        {
            auto& os = (quality == 2 ? *os4x : *os2x);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                (size_t) channels, (size_t) numSamples);
            auto up = os.processSamplesUp (block);

            const int upN = (int) up.getNumSamples();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                for (int i = 0; i < upN; ++i)
                    d[i] = shape (d[i], driveGain, ch);
            }

            os.processSamplesDown (block);
        }

        // Equal-onset blend: amount 0 → dry, amount 1 → fully shaped.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const auto* dry = blendScratch.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = math::lerp (dry[i], d[i], amount);
        }
    }

private:
    float shape (float x, float driveGain, int ch) noexcept
    {
        switch (character)
        {
            case Character::Hard:
                return math::clamp (driveGain * x, -1.0f, 1.0f);

            case Character::Asymmetric:
            {
                const float g  = x >= 0.0f ? driveGain : driveGain * 0.6f;
                const float y  = std::tanh (g * x) / std::tanh (driveGain);
                const float out = y - dcX1[ch] + 0.9995f * dcY1[ch];   // remove the DC the asymmetry adds
                dcX1[ch] = y;
                dcY1[ch] = out;
                return out;
            }

            case Character::Soft:
            default:
                return std::tanh (driveGain * x) / std::tanh (driveGain);
        }
    }

    static constexpr float maxDriveDb = 18.0f;

    Character character = Character::Soft;
    int       quality   = 0;                 // 0 off, 1 = 2x, 2 = 4x
    int       numChannels = 2;
    int       maxBlock  = 512;
    double    baseSampleRate = 44100.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2x, os4x;
    juce::AudioBuffer<float> blendScratch;

    SmoothedParameter amountSmoothed;
    float dcX1[2] = { 0.0f, 0.0f };
    float dcY1[2] = { 0.0f, 0.0f };
};

} // namespace zs
