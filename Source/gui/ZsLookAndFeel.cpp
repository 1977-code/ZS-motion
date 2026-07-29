#include "ZsLookAndFeel.h"

namespace zs
{

using namespace juce;

ZsLookAndFeel::ZsLookAndFeel()
{
    setColour (TextButton::buttonColourId,        theme::panel);
    setColour (TextButton::buttonOnColourId,      theme::gold);
    setColour (TextButton::textColourOffId,       theme::textMuted);
    setColour (TextButton::textColourOnId,        theme::background);

    setColour (TextEditor::backgroundColourId,    theme::panelDeep);
    setColour (TextEditor::textColourId,          theme::text);
    setColour (TextEditor::highlightColourId,     theme::gold.withAlpha (0.30f));
    setColour (TextEditor::highlightedTextColourId, theme::text);
    setColour (TextEditor::outlineColourId,       theme::gold.withAlpha (0.55f));
    setColour (TextEditor::focusedOutlineColourId, theme::gold);
    setColour (CaretComponent::caretColourId,     theme::gold);

    setColour (Label::textColourId,               theme::text);
    setColour (Label::backgroundColourId,         Colours::transparentBlack);
    setColour (Label::outlineColourId,            Colours::transparentBlack);

    setColour (TooltipWindow::backgroundColourId, theme::panel);
    setColour (TooltipWindow::textColourId,       theme::text);
    setColour (TooltipWindow::outlineColourId,    theme::border);
}

//==============================================================================
void ZsLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float rotaryStartAngle,
                                      float rotaryEndAngle, Slider& slider)
{
    const auto area   = Rectangle<int> (x, y, width, height).toFloat();
    const auto size   = jmin (area.getWidth(), area.getHeight());
    const auto centre = area.getCentre();
    const auto radius = size * 0.5f - 2.0f;

    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto trackRadius = radius - 3.0f;
    const auto trackWidth  = jmax (2.0f, size * 0.055f);

    const bool active = slider.isEnabled();

    //--- outer halo, brighter the further the knob is turned --------------------
    if (sliderPos > 0.001f && active)
    {
        auto glow = ColourGradient (theme::gold.withAlpha (0.13f * sliderPos), centre,
                                    Colours::transparentBlack, centre.translated (radius * 1.9f, 0.0f),
                                    true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (radius * 3.8f, radius * 3.8f).withCentre (centre));
    }

    //--- background arc --------------------------------------------------------
    Path track;
    track.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::border);
    g.strokePath (track, PathStrokeType (trackWidth, PathStrokeType::curved, PathStrokeType::rounded));

    //--- value arc -------------------------------------------------------------
    if (sliderPos > 0.0015f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                             rotaryStartAngle, angle, true);

        g.setColour (theme::gold.withAlpha (active ? 0.22f : 0.08f));
        g.strokePath (value, PathStrokeType (trackWidth * 2.4f, PathStrokeType::curved,
                                             PathStrokeType::rounded));

        g.setColour (active ? theme::gold : theme::goldDeep);
        g.strokePath (value, PathStrokeType (trackWidth, PathStrokeType::curved,
                                             PathStrokeType::rounded));
    }

    //--- body ------------------------------------------------------------------
    const auto bodyRadius = trackRadius - trackWidth * 0.5f - 4.0f;
    const auto body       = Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

    g.setGradientFill (ColourGradient (Colour (0xff181614), body.getCentreX(), body.getY(),
                                       Colour (0xff050505), body.getCentreX(), body.getBottom(), false));
    g.fillEllipse (body);

    g.setColour (theme::borderBright);
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    //--- pointer ---------------------------------------------------------------
    Path pointer;
    pointer.startNewSubPath (0.0f, -bodyRadius * 0.40f);
    pointer.lineTo          (0.0f, -bodyRadius * 0.86f);

    g.setColour (active ? theme::goldLight : theme::textFaint);
    g.strokePath (pointer, PathStrokeType (jmax (1.6f, size * 0.032f), PathStrokeType::curved,
                                           PathStrokeType::rounded),
                  AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
void ZsLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour&,
                                          bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = jmin (6.0f, bounds.getHeight() * 0.22f);
    const bool on     = button.getToggleState();

    if (on)
    {
        g.setColour (theme::gold.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds.expanded (2.0f), corner + 2.0f);
    }

    g.setGradientFill (ColourGradient (on ? theme::gold.withAlpha (0.22f) : Colour (0xff111010),
                                       bounds.getCentreX(), bounds.getY(),
                                       on ? theme::gold.withAlpha (0.10f) : Colour (0xff070707),
                                       bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, corner);

    if (isDown)
    {
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds, corner);
    }

    auto outline = on ? theme::gold
                      : (isHighlighted ? theme::gold.withAlpha (0.45f) : theme::border);

    g.setColour (outline);
    g.drawRoundedRectangle (bounds, corner, on ? 1.4f : 1.0f);
}

void ZsLookAndFeel::drawButtonText (Graphics& g, TextButton& button, bool isHighlighted, bool)
{
    const bool on = button.getToggleState();

    auto colour = on ? theme::goldLight
                     : (isHighlighted ? theme::text : theme::textMuted);

    if (! button.isEnabled())
        colour = theme::textFaint;

    auto font = getTextButtonFont (button, button.getHeight());
    font.setExtraKerningFactor (0.16f);

    g.setFont (font);
    g.setColour (colour);
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(),
                Justification::centred, false);
}

Font ZsLookAndFeel::getTextButtonFont (TextButton&, int buttonHeight)
{
    return theme::display (jlimit (10.0f, 15.0f, (float) buttonHeight * 0.36f));
}

Font ZsLookAndFeel::getLabelFont (Label& label)
{
    return theme::value ((float) jlimit (11, 20, label.getHeight() - 6));
}

//==============================================================================
void ZsLookAndFeel::fillTextEditorBackground (Graphics& g, int width, int height, TextEditor&)
{
    g.setColour (theme::panelDeep);
    g.fillRoundedRectangle (Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 4.0f);
}

void ZsLookAndFeel::drawTextEditorOutline (Graphics& g, int width, int height, TextEditor&)
{
    g.setColour (theme::gold.withAlpha (0.8f));
    g.drawRoundedRectangle (Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f),
                            4.0f, 1.0f);
}

//==============================================================================
void ZsLookAndFeel::drawCornerResizer (Graphics& g, int width, int height,
                                       bool isMouseOver, bool isMouseDragging)
{
    const auto alpha = isMouseDragging ? 0.95f : (isMouseOver ? 0.7f : 0.35f);
    g.setColour (theme::gold.withAlpha (alpha));

    for (int i = 1; i <= 3; ++i)
    {
        const auto inset = (float) i * 4.5f;
        g.drawLine ((float) width - inset, (float) height - 3.0f,
                    (float) width - 3.0f,  (float) height - inset, 1.2f);
    }
}

//==============================================================================
Rectangle<int> ZsLookAndFeel::getTooltipBounds (const String& tipText, Point<int> screenPos,
                                                Rectangle<int> parentArea)
{
    const auto font = theme::label (12.0f);

    AttributedString s;
    s.setJustification (Justification::centredLeft);
    s.append (tipText, font, theme::text);

    TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 340.0f);

    const int w = (int) (layout.getWidth() + 20.0f);
    const int h = (int) (layout.getHeight() + 14.0f);

    return Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 18,
                           screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                           w, h)
             .constrainedWithin (parentArea);
}

void ZsLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    const auto bounds = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (theme::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    AttributedString s;
    s.setJustification (Justification::centred);
    s.append (text, theme::label (12.0f), theme::text);

    TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, (float) width - 20.0f);
    layout.draw (g, bounds.reduced (10.0f, 7.0f));
}

} // namespace zs
