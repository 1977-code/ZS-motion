#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

#include "Theme.h"
#include "../PluginProcessor.h"
#include "../dsp/LFO.h"

namespace zs
{

/**
    The centrepiece: a kinetic sculpture that *is* the modulation.

    The figure is a closed polar curve whose radial profile is the selected LFO
    waveform — the same `waveformShape()` the audio thread runs, so a square wave
    really does turn the rotor into a stepped cog and a saw into a ratchet. Nothing
    here is decorative animation; every part is a reading of the DSP:

        rotation        — the LFO phase, taken from the processor and phase-locked,
                          so the sculpture turns at exactly the Rate
        lobe amplitude  — Depth
        radial profile  — Waveform
        colour / bloom  — Saturation (the metal runs hot as it is driven)
        L/R separation  — Width, with the two bodies pulled apart by Stereo phase
        hub pulse       — output level
        blade strobe    — the Fan, at its own carrier phase and blade count
        two rotors      — Rotary mode, horn outside and drum inside, at their own
                          speeds, so the inertia of a spin-up is visible

    It is also a control: drag it vertically for Depth, horizontally for Rate,
    wheel to trim the Rate, double-click to reset both.
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

    /** The radial profile at `t` turns around the figure, in [-1, 1]. */
    float profileAt (Waveform wave, float t, int lobes) const noexcept;

    juce::Path rotorPath (juce::Point<float> centre, float radius, float depth,
                          Waveform wave, int lobes, float rotation) const;

    void paintPanel     (juce::Graphics&, juce::Rectangle<float> area) const;
    void paintGraticule (juce::Graphics&, juce::Rectangle<float> area,
                         juce::Point<float> centre, float radius, float glow) const;
    void paintTrace     (juce::Graphics&, juce::Rectangle<float> strip,
                         Waveform, float stereoOffset, juce::Colour) const;
    void paintBody     (juce::Graphics&, juce::Point<float> centre, float radius,
                        float depth, Waveform, int lobes, float rotation,
                        juce::Colour colour, float trail) const;
    void paintFan      (juce::Graphics&, juce::Point<float> centre, float radius) const;
    void paintHub      (juce::Graphics&, juce::Point<float> centre, float radius,
                        float level, juce::Colour) const;
    void paintReadout  (juce::Graphics&, juce::Rectangle<float> area) const;

    float param (const char* id) const noexcept;
    void  setParam (const char* id, float plainValue) const;

    //==========================================================================
    static constexpr int   lobeCount      = 5;    // reads clearly at any size
    static constexpr int   rotaryLobes    = 2;    // a horn is a dipole
    static constexpr int   pathSteps      = 260;
    static constexpr int   trailSteps     = 3;
    static constexpr float refreshHz      = 50.0f;
    static constexpr int   traceHeight    = 46;   // the linear scope along the bottom
    static constexpr int   traceCycles    = 2;

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
    int   cycleIndex   = 0;      // which of the two drawn cycles the sweep is in

    std::array<float, 16> randomNodes {};

    bool  dragging = false;
    float dragStartDepth = 0.0f, dragStartRate = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotorView)
};

} // namespace zs
