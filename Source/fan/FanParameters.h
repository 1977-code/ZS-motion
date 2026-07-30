#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../ParameterRanges.h"

/**
    The control surface of ZS-MOTION-FAN — ten controls, matching the reference
    plug-in's shape rather than the sibling's twenty-four.

    That is the whole point of this build. ZS-motion is the deep one: stereo phase,
    global phase, output trim, feedback, oversampling, six fan controls. This one is
    the opposite by design — you get a shape, a speed, a depth, and a switch, and
    the character is baked in. Both engines are ours; only the reach differs.

    Ranges taken from measurement, not from the reference's documentation, which
    understates the rate: the manual says 0–10 Hz while the parameter actually runs
    0–32 Hz, linearly (normalised 0.5 reads 16.00 Hz, 1.0 reads 32.00 Hz).
*/
namespace zs::fanparams
{
    inline constexpr const char* bypass     = "bypass";
    inline constexpr const char* active     = "active";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* depth      = "depth";
    inline constexpr const char* rateHz     = "rateHz";
    inline constexpr const char* rateSync   = "rateSync";
    inline constexpr const char* syncOn     = "syncOn";
    inline constexpr const char* morph      = "morph";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* width      = "width";
    inline constexpr const char* fanOn      = "fanOn";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* modifier   = "modifier";

    /** Measured range of the reference: linear, and three times what its manual claims. */
    inline constexpr float rateMaxHz     = 32.0f;
    inline constexpr float rateDefaultHz = 1.0f;

    /** "None" reproduces the reference's un-lit mode button: the fan alone, with no
        modulation in front of it. Theirs is not automatable, so it never shows up in
        a parameter list — ours is, which is strictly more useful. */
    inline juce::StringArray modeChoices()     { return { "None", "Chorus", "Vibrato", "Rotary", "Tremolo" }; }
    inline juce::StringArray divisionChoices() { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }; }
    inline juce::StringArray modifierChoices() { return { "Full", "Dotted", "Triplet" }; }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;

        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { bypass, 1 }, "Bypass", false));

        // Measured, not assumed: with Active off the reference passes the input
        // through untouched — fan included. It is a master on/off, not a
        // modulation-only switch, so this one behaves the same way.
        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { active, 1 }, "Active", true));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { mode, 1 }, "Mode", modeChoices(), 1));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { depth, 1 }, "Depth",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        // Linear, exactly as measured — no skew.
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { rateHz, 1 }, "Rate Hz",
            NormalisableRange<float> { 0.0f, rateMaxHz }, rateDefaultHz,
            AudioParameterFloatAttributes()
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));

        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { syncOn, 1 }, "Sync", false));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { rateSync, 1 }, "Rate BPM", divisionChoices(), 2));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { modifier, 1 }, "Modifier", modifierChoices(), 0));

        /** One knob for the shape, no steps — the reference's Morph. */
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { morph, 1 }, "Morph",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { saturation, 1 }, "Saturation",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { width, 1 }, "Width",
            NormalisableRange<float> { 0.0f, 2.0f }, 1.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));

        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { fanOn, 1 }, "Fan On", false));

        // Wet by default, like the reference.
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { mix, 1 }, "Mix",
            NormalisableRange<float> { 0.0f, 1.0f }, 1.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        return { p.begin(), p.end() };
    }
} // namespace zs::fanparams
