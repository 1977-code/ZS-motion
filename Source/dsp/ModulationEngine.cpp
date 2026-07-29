#include "ModulationEngine.h"

namespace zs
{

void ModulationEngine::prepare (double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = juce::jmax (1, channels);

    lfo.prepare (sampleRate);

    chorus.prepare  (sampleRate, maximumBlockSize);
    vibrato.prepare (sampleRate, maximumBlockSize);
    tremolo.prepare (sampleRate, maximumBlockSize);
    rotary.prepare  (sampleRate, maximumBlockSize);

    fan.prepare (sampleRate);
    saturator.prepare (sampleRate, maximumBlockSize, numChannels);
    stereoWidth.prepare (sampleRate);
    mixer.prepare (sampleRate);

    outputGain.prepare (sampleRate, 0.01f, 1.0f);

    wetScratch.setSize (numChannels, juce::jmax (1, maximumBlockSize));

    xfadeLen       = juce::jmax (1, (int) (0.03 * sampleRate));   // ~30 ms
    xfadeCountdown = 0;

    currentMode = pending.mode;
    prevMode    = pending.mode;

    reset();
}

void ModulationEngine::reset()
{
    lfo.reset (0.0f);
    chorus.reset();
    vibrato.reset();
    tremolo.reset();
    rotary.reset();
    fan.reset();
    saturator.reset();
    stereoWidth.reset();
    mixer.reset();

    xfadeCountdown = 0;

    uiPhase.store (0.0f, std::memory_order_relaxed);
    uiLfo.store   (0.0f, std::memory_order_relaxed);
    uiLevel.store (0.0f, std::memory_order_relaxed);
}

ModeProcessor* ModulationEngine::modeFor (Mode m) noexcept
{
    switch (m)
    {
        case Mode::Chorus:  return &chorus;
        case Mode::Vibrato: return &vibrato;
        case Mode::Tremolo: return &tremolo;
        case Mode::Rotary:  return &rotary;
        case Mode::numModes:
        default:            return &chorus;
    }
}

void ModulationEngine::applySettings (const Settings& s) noexcept
{
    lfo.setFrequency (s.rateHz);
    lfo.setWaveform  ((Waveform) juce::jlimit (0, (int) Waveform::numWaveforms - 1, s.waveform));

    chorus.setDepth (s.depth);
    chorus.setFeedback (s.chorusFeedback);
    vibrato.setDepth (s.depth);
    tremolo.setDepth (s.depth);
    rotary.setDepth (s.depth);
    rotary.setRateHz (s.rateHz);

    fan.setRateHz (s.fanRateHz);
    fan.setAmount (s.fanEnabled ? s.fanAmount : 0.0f);
    fan.setBlades (s.fanBlades);
    fan.setIrregularity (s.irregularity);
    fan.setResonance (s.fanResonance);
    fan.setShape (s.fanShape);

    saturator.setAmount (s.saturation);
    saturator.setCharacter ((Saturator::Character) juce::jlimit (0, 2, s.satCharacter));
    saturator.setQuality (s.satQuality);

    stereoWidth.setWidth (s.width);
    mixer.setMix (s.mix);
    outputGain.setTarget (math::dbToGain (s.outputDb));
}

void ModulationEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const Settings s = pending;
    applySettings (s);

    const int channels   = juce::jmin (numChannels, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    if (channels <= 0 || numSamples <= 0)
        return;

    // MODE change → start a crossfade from the outgoing engine.
    if (s.mode != currentMode)
    {
        prevMode       = currentMode;
        currentMode    = s.mode;
        xfadeCountdown = xfadeLen;
    }

    if (s.hostPhase01 >= 0.0f)
        lfo.setPhase (s.hostPhase01);

    const float phaseOffset01  = s.phaseDeg / 360.0f;
    const float stereoOffset01 = s.stereoPhaseDeg / 360.0f;

    ModeProcessor* cur  = modeFor (currentMode);
    ModeProcessor* prev = modeFor (prevMode);

    const bool stereo = channels > 1;
    float* left  = buffer.getWritePointer (0);
    float* right = stereo ? buffer.getWritePointer (1) : nullptr;

    // Wet path opens with saturation, run as a block stage so it can be
    // oversampled. Work on a scratch copy of the input; the dry stays in buffer.
    for (int ch = 0; ch < channels; ++ch)
        wetScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    {
        juce::AudioBuffer<float> wetView (wetScratch.getArrayOfWritePointers(), channels, numSamples);
        saturator.processBlock (wetView);
    }

    const float* satL = wetScratch.getReadPointer (0);
    const float* satR = stereo ? wetScratch.getReadPointer (1) : satL;

    float blockPeak = 0.0f;
    float lastLfo   = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = left[i];
        const float dryR = stereo ? right[i] : dryL;

        lfo.advance();
        const float lfoL = lfo.value (phaseOffset01);
        const float lfoR = lfo.value (phaseOffset01 + stereoOffset01);
        lastLfo = lfoL;

        const float sL = satL[i];
        const float sR = satR[i];

        // Current mode (+ outgoing mode during a crossfade).
        float cL, cR;
        cur->processSample (sL, sR, lfoL, lfoR, cL, cR);

        float mL = cL, mR = cR;

        if (xfadeCountdown > 0)
        {
            float pL, pR;
            prev->processSample (sL, sR, lfoL, lfoR, pL, pR);

            const float pg = (float) xfadeCountdown / (float) xfadeLen;
            mL = cL * (1.0f - pg) + pL * pg;
            mR = cR * (1.0f - pg) + pR * pg;
            --xfadeCountdown;
        }

        // Fan → width → dry/wet → output gain.
        float fL, fR;
        fan.processSample (mL, mR, fL, fR);

        float wL, wR;
        stereoWidth.processSample (fL, fR, wL, wR);

        float oL, oR;
        mixer.processSample (dryL, dryR, wL, wR, oL, oR);

        const float og = outputGain.getNextValue();
        oL *= og;
        oR *= og;

        left[i] = oL;
        if (stereo)
            right[i] = oR;

        blockPeak = juce::jmax (blockPeak, std::abs (oL), std::abs (oR));
    }

    uiPhase.store     (lfo.getPhase(),        std::memory_order_relaxed);
    uiLfo.store       (lastLfo,               std::memory_order_relaxed);
    uiFanPhase.store  (fan.getPhase(),        std::memory_order_relaxed);
    uiHornPhase.store (rotary.getHornPhase(), std::memory_order_relaxed);
    uiDrumPhase.store (rotary.getDrumPhase(), std::memory_order_relaxed);
    uiHornSpeed.store (rotary.getHornSpeed(), std::memory_order_relaxed);

    // Light attack / slow release so the UI meter reads smoothly.
    const float prevLevel = uiLevel.load (std::memory_order_relaxed);
    const float newLevel  = blockPeak > prevLevel ? blockPeak
                                                   : prevLevel * 0.85f + blockPeak * 0.15f;
    uiLevel.store (newLevel, std::memory_order_relaxed);
}

} // namespace zs
