#include "FanEngine.h"

namespace zs::fan
{

void FanEngine::prepare (double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = juce::jmax (1, channels);

    lfo.prepare (sampleRate);
    ringFan.prepare (sampleRate);
    saturator.prepare (sampleRate, maximumBlockSize, numChannels);
    stereoWidth.prepare (sampleRate);
    mixer.prepare (sampleRate);

    const int maxDelaySamples = (int) msToSamples (maxDelayMs) + 4;

    for (auto& line : delays)
        line.prepare (maxDelaySamples);

    for (auto& line : rotaryDelays)
        line.prepare (maxDelaySamples);

    tremoloSlew = 1.0f - std::exp (-1.0f / (float) (0.0015 * sampleRate));   // ~1.5 ms

    depthSmoothed.prepare (sampleRate, 0.02f, pending.depth);
    activeSmoothed.prepare (sampleRate, 0.03f, pending.active ? 1.0f : 0.0f);

    wetScratch.setSize (numChannels, juce::jmax (1, maximumBlockSize));

    reset();
}

void FanEngine::reset()
{
    lfo.reset (0.0f);
    ringFan.reset();
    saturator.reset();
    stereoWidth.reset();
    mixer.reset();

    for (auto& line : delays)
        line.reset();

    for (auto& line : rotaryDelays)
        line.reset();

    rotaryPhase = 0.0f;
    rotaryDrumPhase = 0.25f;
    tremoloGainL = tremoloGainR = 1.0f;

    uiPhase.store (0.0f, std::memory_order_relaxed);
    uiFanPhase.store (0.0f, std::memory_order_relaxed);
    uiLevel.store (0.0f, std::memory_order_relaxed);
}

void FanEngine::applySettings (const Settings& s) noexcept
{
    lfo.setFrequency (s.rateHz);
    lfo.setMorph (s.morph);

    // Depth drives the carrier itself here: at zero the fan output is silence, not
    // the dry signal. That is what the reference does, and it is why its manual
    // tells you to keep the mix at half or below.
    ringFan.setRateHz (s.rateHz);
    ringFan.setDepth (s.fanOn ? s.depth : 0.0f);

    saturator.setAmount (s.saturation);
    saturator.setCharacter (Saturator::Character::Soft);
    saturator.setQuality (0);                 // no oversampling: no latency

    stereoWidth.setWidth (s.width);
    mixer.setMix (s.mix);

    depthSmoothed.setTarget (s.depth);
    activeSmoothed.setTarget (s.active ? 1.0f : 0.0f);
}

void FanEngine::renderDelayMode (float inL, float inR, float lfoL, float lfoR,
                                 float depth, bool keepDry, float& outL, float& outR) noexcept
{
    const float baseMs = keepDry ? chorusBaseMs : vibratoBaseMs;
    const float modMs  = (keepDry ? chorusModMs : vibratoModMs) * depth;

    const float dL = msToSamples (baseMs + modMs * lfoL);
    const float dR = msToSamples (baseMs + modMs * lfoR);

    // One voice, no feedback — the reference duplicates the input once and detunes
    // that copy, and its impulse response shows no repeats.
    outL = delays[0].processSample (inL, dL);
    outR = delays[1].processSample (inR, dR);

    if (keepDry)
    {
        // Chorus is the detuned copy against the original; vibrato is the copy alone.
        outL = 0.5f * (inL + outL);
        outR = 0.5f * (inR + outR);
    }
}

void FanEngine::renderRotary (float inL, float inR, float depth, float& outL, float& outR) noexcept
{
    const float mono = 0.5f * (inL + inR);

    const float angle = math::twoPi * rotaryPhase;
    const float drum  = math::twoPi * rotaryDrumPhase;

    const float s = std::sin (angle);
    const float c = std::cos (angle);

    // Pitch, volume and panning together, as its manual describes.
    const float wet = rotaryDelays[0].processSample (
        mono, msToSamples (rotaryBaseMs + rotaryDopplerMs * depth * s));

    const float drumWet = rotaryDelays[1].processSample (
        mono, msToSamples (rotaryBaseMs + rotaryDopplerMs * 0.5f * depth * std::sin (drum)));

    const float amplitude = 1.0f - 0.5f * depth + 0.5f * depth * (0.5f + 0.5f * c);

    const float panAngle = ((s * depth) * 0.5f + 0.5f) * math::halfPi;

    const float body = 0.7f * wet + 0.3f * drumWet;

    outL = body * amplitude * std::cos (panAngle);
    outR = body * amplitude * std::sin (panAngle);
}

void FanEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const Settings s = pending;
    applySettings (s);

    const int channels   = juce::jmin (numChannels, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    if (channels <= 0 || numSamples <= 0)
        return;

    const bool stereo = channels > 1;
    float* left  = buffer.getWritePointer (0);
    float* right = stereo ? buffer.getWritePointer (1) : nullptr;

    // Bypass and the panel's own on/off do the same thing, as they do in the
    // reference: the input passes straight through. There is no oversampling here,
    // so there is no latency to make up for either.
    if (s.bypassed || ! s.active)
    {
        uiLevel.store (0.0f, std::memory_order_relaxed);
        return;
    }

    if (s.hostPhase01 >= 0.0f)
        lfo.setPhase (s.hostPhase01);

    // The wet path opens with saturation, run over the block.
    for (int ch = 0; ch < channels; ++ch)
        wetScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    {
        juce::AudioBuffer<float> view (wetScratch.getArrayOfWritePointers(), channels, numSamples);
        saturator.processBlock (view);
    }

    const float* satL = wetScratch.getReadPointer (0);
    const float* satR = stereo ? wetScratch.getReadPointer (1) : satL;

    const float rotaryInc = (float) (s.rateHz / sampleRate);

    float blockPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = left[i];
        const float dryR = stereo ? right[i] : dryL;

        lfo.advance();

        const float depth = depthSmoothed.getNextValue();

        const float lfoL = lfo.value (0.0f);
        const float lfoR = lfo.value (0.25f);      // fixed quarter-turn spread

        const float sL = satL[i];
        const float sR = stereo ? satR[i] : sL;

        float mL = sL, mR = sR;

        switch (s.mode)
        {
            case Mode::None:
                // The fan on its own — nothing in front of it, so the ring
                // modulator gets a clean signal and nulls the carrier properly.
                break;

            case Mode::Chorus:
                renderDelayMode (sL, sR, lfoL, lfoR, depth, true, mL, mR);
                break;

            case Mode::Vibrato:
                renderDelayMode (sL, sR, lfoL, lfoR, depth, false, mL, mR);
                break;

            case Mode::Rotary:
                rotaryPhase += rotaryInc;
                if (rotaryPhase >= 1.0f) rotaryPhase -= std::floor (rotaryPhase);

                rotaryDrumPhase += rotaryInc * rotaryDrumRatio;
                if (rotaryDrumPhase >= 1.0f) rotaryDrumPhase -= std::floor (rotaryDrumPhase);

                renderRotary (sL, sR, depth, mL, mR);
                break;

            case Mode::Tremolo:
            {
                const float targetL = 1.0f - depth + depth * (lfoL * 0.5f + 0.5f);
                const float targetR = 1.0f - depth + depth * (lfoR * 0.5f + 0.5f);

                tremoloGainL += tremoloSlew * (targetL - tremoloGainL);
                tremoloGainR += tremoloSlew * (targetR - tremoloGainR);

                mL = sL * tremoloGainL;
                mR = sR * tremoloGainR;
                break;
            }
        }

        float fL = mL, fR = mR;

        if (s.fanOn)
            ringFan.processSample (mL, mR, fL, fR);

        float wL, wR;
        stereoWidth.processSample (fL, fR, wL, wR);

        float oL, oR;
        mixer.processSample (dryL, dryR, wL, wR, oL, oR);

        left[i] = oL;

        if (stereo)
            right[i] = oR;

        blockPeak = juce::jmax (blockPeak, std::abs (oL), std::abs (oR));
    }

    uiPhase.store (lfo.getPhase(), std::memory_order_relaxed);
    uiFanPhase.store (lfo.getPhase(), std::memory_order_relaxed);

    const float previous = uiLevel.load (std::memory_order_relaxed);
    uiLevel.store (blockPeak > previous ? blockPeak
                                        : previous * 0.85f + blockPeak * 0.15f,
                   std::memory_order_relaxed);
}

} // namespace zs::fan
