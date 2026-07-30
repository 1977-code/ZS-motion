#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

#include "ParameterRanges.h"

/**
    Parameter identifiers, ranges, choices and defaults for ZS-motion.

    The face of the plug-in stays close to the original: one MODE, Depth, Rate
    (free or tempo-synced), waveform, stereo phase, the Fan, saturation, width,
    mix and output. A couple of deeper controls (chorus feedback, fan blades and
    irregularity, saturation character) live in the layout too, ready for the
    advanced panel.
*/
namespace zs::params
{
    // ── IDs ──────────────────────────────────────────────────────────────────
    inline constexpr const char* bypass         = "bypass";
    inline constexpr const char* mode           = "mode";
    inline constexpr const char* depth          = "depth";
    inline constexpr const char* rate           = "rate";
    inline constexpr const char* syncEnabled    = "syncEnabled";
    inline constexpr const char* syncDivision   = "syncDivision";
    inline constexpr const char* syncModifier   = "syncModifier";
    inline constexpr const char* waveform       = "waveform";
    inline constexpr const char* phase          = "phase";
    inline constexpr const char* stereoPhase    = "stereoPhase";

    inline constexpr const char* saturation     = "saturation";
    inline constexpr const char* satCharacter   = "satCharacter";
    inline constexpr const char* satQuality     = "satQuality";

    inline constexpr const char* fanEnabled     = "fanEnabled";
    inline constexpr const char* fanAmount      = "fanAmount";
    inline constexpr const char* fanRate        = "fanRate";
    inline constexpr const char* fanBlades      = "fanBlades";
    inline constexpr const char* irregularity   = "irregularity";
    inline constexpr const char* fanShape       = "fanShape";
    inline constexpr const char* fanResonance   = "fanResonance";

    inline constexpr const char* width          = "width";
    inline constexpr const char* mix            = "mix";
    inline constexpr const char* output         = "output";
    inline constexpr const char* chorusFeedback = "chorusFeedback";

    // ── Choice labels ─────────────────────────────────────────────────────────
    inline juce::StringArray modeChoices()      { return { "Chorus", "Vibrato", "Tremolo", "Rotary" }; }
    inline juce::StringArray waveformChoices()  { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" }; }
    inline juce::StringArray divisionChoices()  { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }; }
    inline juce::StringArray modifierChoices()  { return { "Straight", "Dotted", "Triplet" }; }
    inline juce::StringArray characterChoices() { return { "Soft", "Asymmetric", "Hard" }; }
    inline juce::StringArray qualityChoices()   { return { "Off", "2x", "4x" }; }
    inline juce::StringArray fanShapeChoices()  { return { "Sine", "Rounded", "Pulse" }; }

    // ── Ranges ─────────────────────────────────────────────────────────────────
    /** Logarithmic range — equal knob travel gives equal ratio. */
    inline juce::NormalisableRange<float> makeLogRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
    }

    inline juce::String rateToString (float hz, int = 0)
    {
        if (hz < 1.0f) return juce::String (hz, 2) + " Hz";
        return juce::String (hz, 2) + " Hz";
    }

    // ── Layout ──────────────────────────────────────────────────────────────
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;

        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

        auto degrees = [] (float v, int)
        {
            return String (roundToInt (v)) + String::fromUTF8 ("\xc2\xb0");
        };

        // Declared first, and handed to the host as the official bypass, so the
        // host's own bypass button drives this one control.
        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { bypass, 1 }, "Bypass", false));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { mode, 1 }, "Mode", modeChoices(), 0));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { depth, 1 }, "Depth",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { rate, 1 }, "Rate",
            makeLogRange (rateMinHz, rateMaxHz), rateDefaultHz,
            AudioParameterFloatAttributes()
                .withLabel ("Hz")
                .withStringFromValueFunction (rateToString)));

        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { syncEnabled, 1 }, "Sync", false));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { syncDivision, 1 }, "Division", divisionChoices(), 2));   // 1/4

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { syncModifier, 1 }, "Modifier", modifierChoices(), 0));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { waveform, 1 }, "Wave", waveformChoices(), 0));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { phase, 1 }, "Phase",
            NormalisableRange<float> { 0.0f, 360.0f, 1.0f }, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (degrees)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { stereoPhase, 1 }, "Stereo",
            NormalisableRange<float> { 0.0f, 180.0f, 1.0f }, 90.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (degrees)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { saturation, 1 }, "Saturation",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { satCharacter, 1 }, "Character", characterChoices(), 0));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { satQuality, 1 }, "Quality", qualityChoices(), 0));

        p.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { fanEnabled, 1 }, "Fan", false));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { fanAmount, 1 }, "Fan Amount",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.3f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { fanRate, 1 }, "Fan Rate",
            makeLogRange (fanRateMinHz, fanRateMaxHz), fanRateDefaultHz,
            AudioParameterFloatAttributes()
                .withLabel ("Hz")
                .withStringFromValueFunction (rateToString)));

        p.push_back (std::make_unique<AudioParameterInt> (
            ParameterID { fanBlades, 1 }, "Blades", 2, 8, 4));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { irregularity, 1 }, "Irregularity",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.2f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { fanShape, 1 }, "Fan Shape", fanShapeChoices(), 0));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { fanResonance, 1 }, "Fan Res",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { width, 1 }, "Width",
            NormalisableRange<float> { 0.0f, 2.0f }, 1.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { mix, 1 }, "Mix",
            NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { output, 1 }, "Output",
            NormalisableRange<float> { outputMinDb, outputMaxDb, 0.1f }, 0.0f,
            AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { chorusFeedback, 1 }, "Feedback",
            NormalisableRange<float> { -0.95f, 0.95f, 0.01f }, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        return { p.begin(), p.end() };
    }
} // namespace zs::params
