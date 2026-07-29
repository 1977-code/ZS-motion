#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace zs
{

/** A rotary control with its caption above and its value below. */
class ZsKnob final : public juce::Component
{
public:
    ZsKnob (juce::RangedAudioParameter& parameter, juce::String captionText);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider& getSlider() noexcept { return slider; }

    /** Dim the caption and value when the control does not apply to the mode. */
    void setRelevant (bool shouldBeRelevant);

private:
    juce::Slider slider;
    juce::String caption;
    juce::RangedAudioParameter& parameter;
    juce::SliderParameterAttachment attachment;
    bool relevant = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZsKnob)
};

} // namespace zs
