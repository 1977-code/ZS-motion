#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

#include "MorphLFO.h"
#include "RingFan.h"
#include "../dsp/FractionalDelay.h"
#include "../dsp/Saturator.h"
#include "../dsp/StereoWidth.h"
#include "../dsp/DryWetMixer.h"
#include "../utils/SmoothedParameter.h"

namespace zs::fan
{

/**
    The whole of ZS-MOTION-FAN:

        in ─┬───────────────────────────── dry ──────────┐
            └─ saturate ─ mode ─ ring fan ─ width ─ wet ──┴─ equal-power mix

    Deliberately narrower than the sibling's engine. Four modes off one morphing
    LFO, a ring-modulating fan, saturation, width, mix. No oversampling, so the
    latency is nought — the reference advertises none and this matches it.

    Two measured numbers set the character apart from ZS-motion:

      • the chorus runs on a **5.6 ms** tap, against the sibling's 11 and 15 ms.
        An impulse through the reference at half depth peaks 268 samples late at
        48 kHz; ours used to land at 542–719. Short is why theirs reads tight and
        discreet where the sibling reads wide.
      • one detuned voice, not two, and no feedback path — as its manual describes
        and its impulse response bears out.
*/
class FanEngine
{
public:
    enum class Mode { None = 0, Chorus, Vibrato, Rotary, Tremolo };

    struct Settings
    {
        bool  bypassed = false;
        bool  active   = true;      // master on/off, measured: off passes dry through
        Mode  mode     = Mode::Chorus;   // None = the fan on its own
        float depth    = 0.5f;
        float rateHz   = 1.0f;
        float morph    = 0.0f;
        float saturation = 0.0f;
        float width    = 1.0f;
        bool  fanOn    = false;
        float mix      = 1.0f;
        float hostPhase01 = -1.0f;
    };

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void setSettings (const Settings& s) noexcept { pending = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept { return 0; }

    float getUiPhase()    const noexcept { return uiPhase.load (std::memory_order_relaxed); }
    float getUiFanPhase() const noexcept { return uiFanPhase.load (std::memory_order_relaxed); }
    float getUiLevel()    const noexcept { return uiLevel.load (std::memory_order_relaxed); }

private:
    void applySettings (const Settings&) noexcept;

    /** Chorus and vibrato: one modulated tap, no second voice and no feedback. */
    void renderDelayMode (float inL, float inR, float lfoL, float lfoR,
                          float depth, bool keepDry, float& outL, float& outR) noexcept;

    void renderRotary (float inL, float inR, float depth, float& outL, float& outR) noexcept;

    float msToSamples (float ms) const noexcept { return ms * 0.001f * (float) sampleRate; }

    /** Measured off the reference: 268 samples at 48 kHz, half depth. */
    static constexpr float chorusBaseMs  = 5.6f;
    static constexpr float chorusModMs   = 3.2f;
    static constexpr float vibratoBaseMs = 5.0f;
    static constexpr float vibratoModMs  = 4.2f;
    static constexpr float maxDelayMs    = 14.0f;

    static constexpr float rotaryBaseMs    = 2.4f;
    static constexpr float rotaryDopplerMs = 1.5f;
    static constexpr float rotaryDrumRatio = 0.75f;

    double sampleRate  = 44100.0;
    int    numChannels = 2;

    MorphLFO lfo;
    RingFan  ringFan;
    Saturator   saturator;
    StereoWidth stereoWidth;
    DryWetMixer mixer;

    std::array<FractionalDelay, 2> delays;
    std::array<FractionalDelay, 2> rotaryDelays;

    float rotaryPhase = 0.0f, rotaryDrumPhase = 0.25f;
    float tremoloGainL = 1.0f, tremoloGainR = 1.0f;
    float tremoloSlew = 0.05f;

    SmoothedParameter depthSmoothed, activeSmoothed;

    juce::AudioBuffer<float> wetScratch;

    Settings pending {};

    std::atomic<float> uiPhase { 0.0f }, uiFanPhase { 0.0f }, uiLevel { 0.0f };

    JUCE_LEAK_DETECTOR (FanEngine)
};

} // namespace zs::fan
