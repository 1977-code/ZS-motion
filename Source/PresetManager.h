#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#include "Parameters.h"

namespace zs
{

/**
    Presets, both the factory set and the user's own.

    Factory presets are plain lists of real-world parameter values held in the
    binary. Loading one first resets every parameter to its default and then
    applies the preset's overrides, so a preset always lands in the same place
    whatever was set before it.

    User presets are XML files under the platform's usual preset folder, so they
    survive an update of the plug-in and can be copied between machines:

        macOS    ~/Library/Audio/Presets/ZS Records/ZS-motion
        Windows  %APPDATA%\ZS Records\ZS-motion\Presets

    Anything the user touches after loading marks the preset dirty, which the
    interface shows, so it is never a mystery whether what you hear is still the
    preset you picked.
*/
class PresetManager final : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToUse)
        : state (stateToUse), factory (buildFactoryPresets())
    {
        for (auto* p : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                state.addParameterListener (ranged->paramID, this);

        refreshUserPresets();
    }

    ~PresetManager() override
    {
        for (auto* p : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                state.removeParameterListener (ranged->paramID, this);
    }

    //==========================================================================
    int getNumFactory() const noexcept { return (int) factory.size(); }
    int getNumUser()    const noexcept { return userFiles.size(); }
    int getNumPresets() const noexcept { return getNumFactory() + getNumUser(); }

    juce::StringArray getFactoryNames() const
    {
        juce::StringArray names;
        for (auto& p : factory)
            names.add (p.name);
        return names;
    }

    juce::StringArray getUserNames() const
    {
        juce::StringArray names;
        for (auto& f : userFiles)
            names.add (f.getFileNameWithoutExtension());
        return names;
    }

    /** Indices run over the factory set first, then the user's. */
    bool isUserPreset (int index) const noexcept { return index >= getNumFactory(); }

    int  getCurrentIndex() const noexcept { return currentIndex; }
    bool isDirty()         const noexcept { return dirty; }

    juce::String getCurrentName() const
    {
        if (currentIndex < 0)
            return "—";

        if (! isUserPreset (currentIndex))
            return factory[(size_t) currentIndex].name;

        const int i = currentIndex - getNumFactory();
        return juce::isPositiveAndBelow (i, userFiles.size())
                 ? userFiles[i].getFileNameWithoutExtension()
                 : "—";
    }

    //==========================================================================
    void load (int index)
    {
        if (index < 0 || index >= getNumPresets())
            return;

        const juce::ScopedValueSetter<bool> quiet (loading, true);

        if (! isUserPreset (index))
        {
            resetToDefaults();

            for (auto& [id, plain] : factory[(size_t) index].values)
                if (auto* p = state.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (plain));
        }
        else
        {
            const auto file = userFiles[index - getNumFactory()];

            if (auto xml = juce::XmlDocument::parse (file))
            {
                if (xml->hasTagName (state.state.getType()))
                {
                    resetToDefaults();
                    applyTree (juce::ValueTree::fromXml (*xml));
                }
            }
        }

        currentIndex = index;
        dirty = false;
    }

    void selectNext()     { if (getNumPresets() > 0) load ((currentIndex + 1) % getNumPresets()); }
    void selectPrevious() { if (getNumPresets() > 0) load ((currentIndex - 1 + getNumPresets()) % getNumPresets()); }

    /** Jump somewhere else at random — the quickest way to find a sound you would
        not have dialled up on purpose. Never lands on where it already is. */
    void selectRandom()
    {
        const int count = getNumPresets();

        if (count <= 1)
            return;

        int next = currentIndex;

        while (next == currentIndex)
            next = random.nextInt (count);

        load (next);
    }

    /** Writes the current settings as a user preset. Returns its new index, or -1. */
    int saveUserPreset (const juce::String& rawName)
    {
        const auto name = rawName.trim();

        if (name.isEmpty())
            return -1;

        const auto directory = getUserDirectory();

        if (! directory.createDirectory())
            return -1;

        const auto file = directory.getChildFile (juce::File::createLegalFileName (name) + ".xml");

        // Only the parameters travel; the window size is this machine's business.
        auto tree = state.copyState();
        tree.removeProperty ("editorWidth", nullptr);
        tree.removeProperty ("editorHeight", nullptr);

        if (auto xml = tree.createXml())
            if (! xml->writeTo (file))
                return -1;

        refreshUserPresets();

        for (int i = 0; i < userFiles.size(); ++i)
            if (userFiles[i] == file)
            {
                currentIndex = getNumFactory() + i;
                dirty = false;
                return currentIndex;
            }

        return -1;
    }

    /** Deletes a user preset. Factory presets are left alone. */
    bool deleteUserPreset (int index)
    {
        if (! isUserPreset (index))
            return false;

        const int i = index - getNumFactory();

        if (! juce::isPositiveAndBelow (i, userFiles.size()))
            return false;

        if (! userFiles[i].deleteFile())
            return false;

        refreshUserPresets();
        currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index);
        return true;
    }

    void refreshUserPresets()
    {
        userFiles.clear();

        const auto directory = getUserDirectory();

        if (directory.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.xml"))
                userFiles.add (entry.getFile());

        // Alphabetical, so the list does not shuffle between launches.
        userFiles.sort();
    }

    static juce::File getUserDirectory()
    {
        const auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

       #if JUCE_MAC
        return root.getChildFile ("Audio/Presets/ZS Records/ZS-motion");
       #else
        return root.getChildFile ("ZS Records/ZS-motion/Presets");
       #endif
    }

private:
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // id → plain value
    };

    void parameterChanged (const juce::String&, float) override
    {
        if (! loading)
            dirty = true;
    }

    void resetToDefaults()
    {
        for (auto* param : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    /** Apply a saved tree through the parameters, so hosts and the interface both
        follow, rather than swapping the tree out underneath them. */
    void applyTree (const juce::ValueTree& tree)
    {
        if (! tree.isValid())
            return;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);

            if (! child.hasType ("PARAM"))
                continue;

            const auto id = child.getProperty ("id").toString();

            if (auto* p = state.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
        }
    }

    static std::vector<Preset> buildFactoryPresets()
    {
        using namespace zs::params;

        // Modes: 0 Chorus, 1 Vibrato, 2 Tremolo, 3 Rotary.
        // Waves: 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square, 5 Random.
        // Divisions: 0 = 1/1 … 5 = 1/32. Character: 0 Soft, 1 Asym, 2 Hard.
        return {
            { "Init", {} },

            //── Chorus ──────────────────────────────────────────────────────────
            { "Lush Chorus", {
                { mode, 0 }, { depth, 0.6f }, { rate, 0.35f }, { stereoPhase, 120.0f },
                { chorusFeedback, 0.2f }, { width, 1.3f }, { mix, 0.45f } } },

            { "Chorus Whisper", {
                { mode, 0 }, { depth, 0.28f }, { rate, 0.22f }, { stereoPhase, 90.0f },
                { width, 1.15f }, { mix, 0.24f } } },

            { "Sixties Wobble", {
                { mode, 0 }, { depth, 0.8f }, { rate, 1.6f }, { waveform, 1 },
                { chorusFeedback, -0.45f }, { stereoPhase, 180.0f }, { mix, 0.6f } } },

            //── Vibrato ─────────────────────────────────────────────────────────
            { "Deep Vibrato", {
                { mode, 1 }, { depth, 0.55f }, { rate, 5.0f }, { mix, 1.0f } } },

            { "Tape Warble", {
                { mode, 1 }, { depth, 0.22f }, { rate, 0.8f }, { waveform, 5 },
                { saturation, 0.25f }, { satCharacter, 1 }, { mix, 1.0f } } },

            //── Tremolo ─────────────────────────────────────────────────────────
            { "Slow Tremolo", {
                { mode, 2 }, { depth, 0.7f }, { rate, 4.0f }, { waveform, 0 },
                { stereoPhase, 90.0f }, { mix, 1.0f } } },

            { "Sync Chop", {
                { mode, 2 }, { depth, 1.0f }, { syncEnabled, 1 }, { syncDivision, 4 },
                { waveform, 4 }, { mix, 1.0f } } },

            { "Triplet Pulse", {
                { mode, 2 }, { depth, 0.85f }, { syncEnabled, 1 }, { syncDivision, 3 },
                { syncModifier, 2 }, { waveform, 1 }, { stereoPhase, 60.0f }, { mix, 1.0f } } },

            { "Helicopter", {
                { mode, 2 }, { depth, 1.0f }, { rate, 8.5f }, { waveform, 4 },
                { stereoPhase, 180.0f }, { width, 1.5f }, { mix, 1.0f } } },

            //── Rotary ──────────────────────────────────────────────────────────
            { "Leslie Slow", {
                { mode, 3 }, { depth, 0.7f }, { rate, 0.8f }, { mix, 1.0f } } },

            { "Leslie Fast", {
                { mode, 3 }, { depth, 0.8f }, { rate, 6.5f }, { mix, 1.0f } } },

            { "Rotary Drive", {
                { mode, 3 }, { depth, 0.75f }, { rate, 4.0f }, { saturation, 0.55f },
                { satCharacter, 1 }, { satQuality, 1 }, { output, -1.5f }, { mix, 1.0f } } },

            //── The Fan ─────────────────────────────────────────────────────────
            { "Fan Whir", {
                { mode, 0 }, { depth, 0.35f }, { rate, 0.5f }, { mix, 0.5f },
                { fanEnabled, 1 }, { fanAmount, 0.5f }, { fanRate, 8.0f }, { fanBlades, 4 },
                { fanShape, 2 }, { fanResonance, 0.3f } } },

            { "Ring Fan", {
                { mode, 0 }, { depth, 0.25f }, { mix, 0.6f },
                { fanEnabled, 1 }, { fanAmount, 0.8f }, { fanRate, 20.0f }, { fanBlades, 6 },
                { fanShape, 0 }, { fanResonance, 0.5f }, { saturation, 0.3f }, { satQuality, 1 } } },

            { "Big Blades", {
                { mode, 1 }, { depth, 0.3f }, { rate, 0.3f }, { mix, 0.75f },
                { fanEnabled, 1 }, { fanAmount, 0.45f }, { fanRate, 3.0f }, { fanBlades, 2 },
                { fanShape, 1 }, { irregularity, 0.55f } } },

            //── Character ───────────────────────────────────────────────────────
            { "Warm Motion", {
                { mode, 0 }, { depth, 0.5f }, { rate, 0.6f }, { saturation, 0.4f },
                { satCharacter, 1 }, { satQuality, 1 }, { width, 1.2f }, { mix, 0.5f },
                { output, 1.0f } } },

            { "Broken Machine", {
                { mode, 2 }, { depth, 0.9f }, { rate, 2.5f }, { waveform, 5 },
                { saturation, 0.9f }, { satCharacter, 2 }, { satQuality, 2 },
                { fanEnabled, 1 }, { fanAmount, 0.6f }, { fanRate, 14.0f }, { fanBlades, 7 },
                { fanShape, 2 }, { irregularity, 0.8f }, { fanResonance, 0.7f },
                { output, -3.0f }, { mix, 1.0f } } },

            { "Mono Collapse", {
                { mode, 0 }, { depth, 0.65f }, { rate, 1.2f }, { width, 0.0f },
                { stereoPhase, 0.0f }, { mix, 0.7f } } },
        };
    }

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> factory;
    juce::Array<juce::File> userFiles;

    juce::Random random;
    int  currentIndex = 0;
    bool dirty   = false;
    bool loading = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace zs
