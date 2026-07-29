#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

#include "LFO.h"
#include "ChorusProcessor.h"
#include "VibratoProcessor.h"
#include "TremoloProcessor.h"
#include "RotaryProcessor.h"
#include "FanModulator.h"
#include "Saturator.h"
#include "StereoWidth.h"
#include "DryWetMixer.h"
#include "../utils/SmoothedParameter.h"

namespace zs
{

/**
    The whole effect, wired as one chain:

        in ─┬─────────────────────────────── dry ────────────────┐
            └─ saturate ─ mode(xfade) ─ fan ─ stereo width ─ wet ─┴─ equal-power
                                                                     mix · output

    One shared LFO drives chorus / vibrato / tremolo; rotary and fan keep their
    own phases (they need several speeds at once). MODE changes are crossfaded
    over ~30 ms so switching never clicks. Everything the audio thread needs is
    prepared once in prepare(); process() allocates nothing.
*/
class ModulationEngine
{
public:
    enum class Mode { Chorus = 0, Vibrato, Tremolo, Rotary, numModes };

    struct Settings
    {
        Mode  mode          = Mode::Chorus;
        float depth         = 0.5f;      // 0..1
        float rateHz        = 1.0f;      // effective LFO rate (sync already resolved)
        int   waveform      = 0;         // zs::Waveform
        float phaseDeg      = 0.0f;      // 0..360 global LFO offset
        float stereoPhaseDeg = 90.0f;    // 0..180

        bool  fanEnabled    = false;
        float fanRateHz     = 6.0f;
        float fanAmount     = 0.3f;
        float fanBlades     = 4.0f;
        float irregularity  = 0.2f;
        float fanResonance  = 0.0f;      // 0..1
        int   fanShape      = 0;         // FanModulator::Shape

        float saturation    = 0.0f;      // 0..1
        int   satCharacter  = 0;         // Saturator::Character
        int   satQuality    = 0;         // 0 off, 1 = 2x, 2 = 4x

        float width         = 1.0f;      // 0..2
        float mix           = 0.5f;      // 0..1
        float outputDb      = 0.0f;      // -24..12
        float chorusFeedback = 0.0f;     // -0.95..0.95

        // Host phase lock: >= 0 forces the LFO phase at block start (sync mode).
        float hostPhase01   = -1.0f;
    };

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void setSettings (const Settings& s) noexcept { pending = s; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Latency the chain currently carries, in samples. Non-zero only while the
        saturation is oversampling; the dry path is delayed to match internally, so
        this is a true figure for the whole plug-in. */
    int getLatencySamples() const noexcept
    {
        return juce::roundToInt (dryDelaySamples);
    }

    // ── UI taps (lock-free) ────────────────────────────────────────────────
    float getUiPhase()      const noexcept { return uiPhase.load (std::memory_order_relaxed); }
    float getUiLfoValue()   const noexcept { return uiLfo.load (std::memory_order_relaxed); }
    float getUiOutputLevel() const noexcept { return uiLevel.load (std::memory_order_relaxed); }
    float getUiFanPhase()   const noexcept { return uiFanPhase.load (std::memory_order_relaxed); }
    float getUiHornPhase()  const noexcept { return uiHornPhase.load (std::memory_order_relaxed); }
    float getUiDrumPhase()  const noexcept { return uiDrumPhase.load (std::memory_order_relaxed); }
    float getUiHornSpeed()  const noexcept { return uiHornSpeed.load (std::memory_order_relaxed); }

private:
    ModeProcessor* modeFor (Mode m) noexcept;
    void applySettings (const Settings& s) noexcept;

    double sampleRate  = 44100.0;
    int    numChannels = 2;

    LFO lfo;
    ChorusProcessor  chorus;
    VibratoProcessor vibrato;
    TremoloProcessor tremolo;
    RotaryProcessor  rotary;

    FanModulator fan;
    Saturator    saturator;
    StereoWidth  stereoWidth;
    DryWetMixer  mixer;

    SmoothedParameter outputGain;

    // Phase offsets are read per block, so they have to be eased or a big jump in
    // Stereo/Phase steps the LFO and clicks.
    SmoothedParameter phaseOffset, stereoOffset;

    // Wet scratch for the block-rate (optionally oversampled) saturation stage.
    juce::AudioBuffer<float> wetScratch;

    // The oversampled saturation delays the wet path by a few samples. The dry
    // going into the mixer is delayed to match, or the two comb against each other.
    std::array<FractionalDelay, 2> dryCompensation;
    float dryDelaySamples = 0.0f;

    Settings pending {};

    Mode currentMode = Mode::Chorus;
    Mode prevMode    = Mode::Chorus;

    // MODE crossfade: prevGain rides 1 → 0 across xfadeLen samples.
    int   xfadeLen      = 1;
    int   xfadeCountdown = 0;

    std::atomic<float> uiPhase { 0.0f };
    std::atomic<float> uiLfo   { 0.0f };
    std::atomic<float> uiLevel { 0.0f };
    std::atomic<float> uiFanPhase  { 0.0f };
    std::atomic<float> uiHornPhase { 0.0f };
    std::atomic<float> uiDrumPhase { 0.0f };
    std::atomic<float> uiHornSpeed { 0.0f };

    JUCE_LEAK_DETECTOR (ModulationEngine)
};

} // namespace zs
