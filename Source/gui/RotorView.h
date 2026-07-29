#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

#include "Theme.h"
#include "../PluginProcessor.h"
#include "../dsp/LFO.h"
#include "../dsp/Saturator.h"

namespace zs
{

/**
    The centrepiece: the machine itself, photographed on a long exposure.

    The body is a closed polar curve whose radial profile is the selected LFO
    waveform — the same `waveformShape()` the audio thread runs — and then pushed
    through the same saturation curve the audio goes through, so driving the
    plug-in really does deform the metal rather than just tint it. Around that:

      body shape        Waveform, and Saturation + Character, which flatten the
                        lobe tips (Hard) or make them lopsided (Asymmetric)
      lobe amplitude    Depth
      rotation          the LFO phase, taken from the processor and phase-locked
      body scale        Output
      two bodies        Width, pulled apart by Stereo phase
      wake              the last two seconds of movement, receding toward the hub,
                        so a knob change is visible as the history changing shape
      flow              particles thrown off the hub and swirled by the rotation —
                        faster with Rate, wilder with Depth, denser with Mix
      dry ring          the plain circle the signal would be without the effect;
                        it fades up as Mix comes down
      echoes            Feedback, as decaying copies of the body
      blades            the Fan: count, shape and rate, chopping the flow as they
                        pass, with a resonance halo
      hub               output level; the rim arc is the same level, held
      index mark        the global Phase offset, and the division when synced
      flash             any parameter move briefly brightens the whole figure, so
                        a change always reads even if it is small

    It is also a control: drag vertically for Depth, horizontally for Rate, wheel
    to trim the Rate, double-click to reset both.
*/
class RotorView final : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit RotorView (ZsMotionAudioProcessor&);

    void paint (juce::Graphics&) override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    //==========================================================================
    /** Everything the drawing needs, read once per frame. */
    struct State
    {
        int   mode = 0;
        Waveform wave = Waveform::Sine;
        float depth = 0.5f, rate = 1.0f, mix = 0.5f, width = 1.0f;
        float stereo = 0.25f, phase = 0.0f, feedback = 0.0f;
        float saturation = 0.0f;
        Saturator::Character character = Saturator::Character::Soft;
        float outputScale = 1.0f;
        bool  fanOn = false;
        float fanAmount = 0.0f, fanRes = 0.0f;
        int   fanBlades = 4, fanShape = 0;
        bool  synced = false;
        int   division = 2, modifier = 0;
        bool  rotary = false;
    };

    State readState() const;

    /** The radial profile at `t` turns around the figure, in [-1, 1]. */
    float profileAt (const State&, float t, int lobes) const noexcept;

    juce::Path rotorPath (const State&, juce::Point<float> centre, float radius,
                          float depth, int lobes, float rotation, int steps) const;

    //==========================================================================
    void updateFlow (const State&, float dt);
    void spawnParticle (int index) noexcept;

    /** How much a blade is covering this angle right now, 0..1. */
    float bladeShadow (const State&, float angle) const noexcept;

    void paintPanel     (juce::Graphics&, juce::Rectangle<float>) const;
    void paintGraticule (juce::Graphics&, const State&, juce::Rectangle<float>,
                         juce::Point<float>, float radius, float glow) const;
    void paintWake      (juce::Graphics&, const State&, juce::Point<float>,
                         float radius, int lobes, juce::Colour) const;
    void paintFlow      (juce::Graphics&, const State&, juce::Point<float>,
                         float radius, juce::Colour) const;
    void paintDryRing   (juce::Graphics&, const State&, juce::Point<float>, float radius) const;
    void paintEchoes    (juce::Graphics&, const State&, juce::Point<float>,
                         float radius, int lobes, float rotation, juce::Colour) const;
    void paintBody      (juce::Graphics&, const State&, juce::Point<float>, float radius,
                         int lobes, float rotation, juce::Colour, float trail, float alpha) const;
    void paintFan       (juce::Graphics&, const State&, juce::Point<float>, float radius) const;
    void paintHub       (juce::Graphics&, juce::Point<float>, float radius,
                         float level, juce::Colour) const;
    void paintTrace     (juce::Graphics&, const State&, juce::Rectangle<float>, juce::Colour) const;
    void paintReadout   (juce::Graphics&, const State&, juce::Rectangle<float>) const;

    float param (const char* id) const noexcept;
    void  setParam (const char* id, float plainValue) const;

    //==========================================================================
    static constexpr int   lobeCount   = 5;    // reads clearly at any size
    static constexpr int   rotaryLobes = 2;    // a horn is a dipole
    static constexpr int   pathSteps   = 220;
    static constexpr int   wakeSteps   = 76;
    static constexpr int   trailSteps  = 3;
    static constexpr float refreshHz   = 50.0f;
    static constexpr int   traceHeight = 46;
    static constexpr int   traceCycles = 2;

    static constexpr int   historySize  = 96;   // ~1.9 s of movement
    static constexpr int   wakeContours = 12;
    static constexpr int   maxParticles = 240;

    //==========================================================================
    ZsMotionAudioProcessor& processor;

    // Local phases, continuously pulled onto the processor's. While audio runs the
    // DSP owns them exactly; when the host is idle they keep turning at the set
    // Rate, so the control still shows what it is doing rather than freezing.
    float displayPhase  = 0.0f;
    float fanPhase      = 0.0f;
    float hornPhase     = 0.0f;
    float drumPhase     = 0.25f;
    float smoothedLevel = 0.0f;

    float lastDspPhase = -1.0f;
    int   staleFrames  = 0;
    int   cycleIndex   = 0;

    std::array<float, 16> randomNodes {};

    /** One frame of the past, for the wake. */
    struct Snapshot { float phase = 0.0f, depth = 0.0f; };
    std::array<Snapshot, historySize> history {};
    int historyWrite = 0, historyCount = 0;

    /** A speck of the flow thrown off the hub. */
    struct Particle
    {
        float radius01 = 0.0f;    // 0 at the hub, 1 at the rim
        float angle    = 0.0f;
        float speed    = 0.0f;
        float spin     = 0.0f;
        float life     = 0.0f;    // 1 → 0
    };
    std::array<Particle, maxParticles> particles {};

    // Any parameter move flashes the figure, so even a small change registers.
    float highlight      = 0.0f;
    float paramSignature = 0.0f;

    math::Xorshift rng { 0x5EEDu };

    bool  dragging = false;
    float dragStartDepth = 0.0f, dragStartRate = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotorView)
};

} // namespace zs
