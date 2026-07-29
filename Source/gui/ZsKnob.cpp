#include "ZsKnob.h"

namespace zs
{

using namespace juce;

namespace
{
    constexpr int captionHeight = 14;
    constexpr int valueHeight   = 20;
}

ZsKnob::ZsKnob (RangedAudioParameter& p, String captionText)
    : caption (std::move (captionText)),
      parameter (p),
      attachment (p, slider, nullptr)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f,
                                MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (true,
        (double) p.convertFrom0to1 (p.getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };

    addAndMakeVisible (slider);
}

void ZsKnob::setRelevant (bool shouldBeRelevant)
{
    if (relevant == shouldBeRelevant)
        return;

    relevant = shouldBeRelevant;
    slider.setEnabled (relevant);
    repaint();
}

void ZsKnob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (captionHeight + 4);
    area.removeFromBottom (valueHeight);
    slider.setBounds (area);
}

void ZsKnob::paint (Graphics& g)
{
    auto area = getLocalBounds();

    auto captionFont = theme::display (10.0f);
    captionFont.setExtraKerningFactor (0.32f);
    g.setFont (captionFont);
    g.setColour (relevant ? theme::textMuted : theme::textFaint);
    g.drawText (caption.toUpperCase(), area.removeFromTop (captionHeight),
                Justification::centred, false);

    g.setFont (theme::value (16.0f));
    g.setColour (relevant ? theme::text : theme::textFaint);
    g.drawText (parameter.getCurrentValueAsText(),
                getLocalBounds().removeFromBottom (valueHeight),
                Justification::centred, false);
}

} // namespace zs
