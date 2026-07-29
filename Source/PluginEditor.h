#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/ZsLookAndFeel.h"
#include "gui/BrandBackground.h"
#include "gui/RotorView.h"
#include "gui/ZsKnob.h"
#include "gui/Layout.h"
#include "gui/Theme.h"

//==============================================================================
/**
    The interface is built once at the fixed logical size in zs::layout and then
    scaled to whatever the window is dragged to, so the proportions never drift and
    every stroke stays sharp.
*/
class ZsMotionAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ZsMotionAudioProcessorEditor (ZsMotionAudioProcessor&);
    ~ZsMotionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    //==========================================================================
    /** The four modes as one row of latching buttons over the choice parameter. */
    struct ModeSelector final : juce::Component
    {
        explicit ModeSelector (juce::RangedAudioParameter&);
        void resized() override;

        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };

    /** A compact output-level bar, so the grid's last cell earns its place. */
    struct LevelMeter final : juce::Component,
                              private juce::Timer
    {
        explicit LevelMeter (ZsMotionAudioProcessor&);
        void paint (juce::Graphics&) override;
        void timerCallback() override;

        ZsMotionAudioProcessor& processor;
        float level = 0.0f, peak = 0.0f;
    };

    /** Everything at logical size; the editor only scales this. */
    struct Content final : juce::Component
    {
        explicit Content (ZsMotionAudioProcessor&);

        void resized() override;

        void refreshRelevance();

        zs::ZsKnob& addKnob (const juce::String& paramID, const juce::String& caption);
        void addCombo (juce::ComboBox&, std::unique_ptr<APVTS::ComboBoxAttachment>&,
                       const juce::String& paramID, const juce::StringArray& items);

        ZsMotionAudioProcessor& processor;

        zs::BrandBackground background;
        zs::RotorView       rotor;
        ModeSelector        modes;
        LevelMeter          meter;

        juce::OwnedArray<zs::ZsKnob> knobs;
        enum KnobIndex { kDepth = 0, kRate, kMix, kWidth, kSaturation, kOutput,
                         kStereo, kPhase, kFeedback, kFanAmount, kFanRate, kBlades, kFanRes };

        juce::ComboBox waveBox, divisionBox, modifierBox, fanShapeBox, characterBox, qualityBox;
        std::unique_ptr<APVTS::ComboBoxAttachment> waveAtt, divisionAtt, modifierAtt,
                                                   fanShapeAtt, characterAtt, qualityAtt;

        juce::TextButton syncButton { "Sync" }, fanButton { "Fan" };
        std::unique_ptr<APVTS::ButtonAttachment> syncAtt, fanAtt;

        juce::ComboBox   presetBox;
        juce::TextButton prevPreset { "<" }, nextPreset { ">" };
        void syncPresetBox();

        // Keeps the greyed-out captions in step with the mode and the Fan switch.
        juce::ParameterAttachment modeWatcher, fanWatcher;
    };

    ZsMotionAudioProcessor& plugin;
    zs::ZsLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 650 };
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZsMotionAudioProcessorEditor)
};
