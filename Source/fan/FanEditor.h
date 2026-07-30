#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "FanProcessor.h"
#include "../gui/ZsLookAndFeel.h"
#include "../gui/ZsKnob.h"
#include "../gui/Theme.h"

//==============================================================================
/**
    A deliberately plain face for a deliberately plain plug-in.

    ZS-motion gets the kinetic sculpture; this one gets one strip that draws the
    thing that actually defines it — the zero-mean blade carrier, with the morphing
    LFO behind it — and then stays out of the way. Ten controls, one row of modes,
    nothing hidden.

    The reference plug-in's own interface is a photographed household fan by an
    outside design studio; none of that is borrowed here. This is the ZS panel
    language shared with the rest of the line.
*/
class ZsFanAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ZsFanAudioProcessorEditor (ZsFanAudioProcessor&);
    ~ZsFanAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    //==========================================================================
    /** The blade carrier, drawn flat: a short pass, a long shaded rest, and the
        sign flip between them that makes this ring rather than throb. */
    struct CarrierView final : juce::Component,
                               private juce::Timer
    {
        explicit CarrierView (ZsFanAudioProcessor&);
        void paint (juce::Graphics&) override;
        void timerCallback() override;

        ZsFanAudioProcessor& processor;
        float sweep = 0.0f;
        float level = 0.0f;
    };

    /** Four modes as one row of latching chips over the choice parameter. */
    struct ModeRow final : juce::Component
    {
        explicit ModeRow (juce::RangedAudioParameter&);
        void resized() override;

        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };

    struct Content final : juce::Component
    {
        explicit Content (ZsFanAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        void addKnob (const juce::String& paramID, const juce::String& caption);
        void addToggle (juce::TextButton&, std::unique_ptr<APVTS::ButtonAttachment>&,
                        const juce::String& paramID, const juce::String& tip);
        void addCombo (juce::ComboBox&, std::unique_ptr<APVTS::ComboBoxAttachment>&,
                       const juce::String& paramID, const juce::StringArray& items);

        ZsFanAudioProcessor& processor;

        CarrierView carrier;
        ModeRow     modes;

        juce::OwnedArray<zs::ZsKnob> knobs;

        juce::TextButton activeButton { "Active" }, fanButton { "Fan" },
                         syncButton { "Sync" }, bypassButton { "Bypass" };
        std::unique_ptr<APVTS::ButtonAttachment> activeAtt, fanAtt, syncAtt, bypassAtt;

        juce::ComboBox divisionBox, modifierBox;
        std::unique_ptr<APVTS::ComboBoxAttachment> divisionAtt, modifierAtt;
    };

    ZsFanAudioProcessor& plugin;
    zs::ZsLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 650 };
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZsFanAudioProcessorEditor)
};

namespace zs::fanlayout
{
    inline constexpr int width  = 760;
    inline constexpr int height = 470;
    inline constexpr int margin = 26;
}
