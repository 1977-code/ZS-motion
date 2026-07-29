#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#include "Parameters.h"

namespace zs
{

/**
    A small factory-preset system over the APVTS.

    Presets are stored as plain (real-world) parameter values; loading one first
    resets every parameter to its default, then applies the preset's overrides,
    so a preset always lands in the same place regardless of the previous state.
    Values are converted to normalised form through each parameter's own range,
    so the logarithmic Rate and the choice/int parameters all map correctly.
*/
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // id → plain value
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToUse)
        : state (stateToUse), presets (buildFactoryPresets()) {}

    int getNumPresets() const noexcept { return (int) presets.size(); }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        for (auto& p : presets)
            names.add (p.name);
        return names;
    }

    int getCurrentIndex() const noexcept { return currentIndex; }

    void load (int index)
    {
        if (index < 0 || index >= (int) presets.size())
            return;

        resetToDefaults();

        for (auto& [id, plain] : presets[(size_t) index].values)
            if (auto* p = state.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (plain));

        currentIndex = index;
    }

    void selectNext()     { load ((currentIndex + 1) % getNumPresets()); }
    void selectPrevious() { load ((currentIndex - 1 + getNumPresets()) % getNumPresets()); }

private:
    void resetToDefaults()
    {
        for (auto* param : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    static std::vector<Preset> buildFactoryPresets()
    {
        using namespace zs::params;

        // Modes: 0 Chorus, 1 Vibrato, 2 Tremolo, 3 Rotary.
        // Waves: 0 Sine .. 4 Square, 5 Random. Divisions: 0=1/1 .. 5=1/32.
        return {
            { "Init", {} },

            { "Lush Chorus", {
                { mode, 0 }, { depth, 0.6f }, { rate, 0.35f }, { stereoPhase, 120.0f },
                { chorusFeedback, 0.2f }, { width, 1.3f }, { mix, 0.45f } } },

            { "Deep Vibrato", {
                { mode, 1 }, { depth, 0.55f }, { rate, 5.0f }, { mix, 1.0f } } },

            { "Slow Tremolo", {
                { mode, 2 }, { depth, 0.7f }, { rate, 4.0f }, { waveform, 0 },
                { stereoPhase, 90.0f }, { mix, 1.0f } } },

            { "Sync Chop", {
                { mode, 2 }, { depth, 1.0f }, { syncEnabled, 1 }, { syncDivision, 4 },
                { waveform, 4 }, { mix, 1.0f } } },

            { "Leslie Slow", {
                { mode, 3 }, { depth, 0.7f }, { rate, 0.8f }, { mix, 1.0f } } },

            { "Leslie Fast", {
                { mode, 3 }, { depth, 0.8f }, { rate, 6.5f }, { mix, 1.0f } } },

            { "Fan Whir", {
                { mode, 0 }, { depth, 0.35f }, { rate, 0.5f }, { mix, 0.5f },
                { fanEnabled, 1 }, { fanAmount, 0.5f }, { fanRate, 8.0f }, { fanBlades, 4 },
                { fanShape, 2 }, { fanResonance, 0.3f } } },

            { "Ring Fan", {
                { mode, 0 }, { depth, 0.25f }, { mix, 0.6f },
                { fanEnabled, 1 }, { fanAmount, 0.8f }, { fanRate, 20.0f }, { fanBlades, 6 },
                { fanShape, 0 }, { fanResonance, 0.5f }, { saturation, 0.3f }, { satQuality, 1 } } },

            { "Warm Motion", {
                { mode, 0 }, { depth, 0.5f }, { rate, 0.6f }, { saturation, 0.4f },
                { satCharacter, 1 }, { satQuality, 1 }, { width, 1.2f }, { mix, 0.5f },
                { output, 1.0f } } },
        };
    }

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> presets;
    int currentIndex = 0;
};

} // namespace zs
