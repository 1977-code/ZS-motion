#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    The ZS Records palette and type, ported from the site's design tokens
    (src/app/globals.css) so the plug-in and zsr.artspace1977.ru read as one
    brand. The site stores colours in OKLCH; the sRGB values below are the
    exact conversions of those tokens.
*/
namespace zs::theme
{
    inline const juce::Colour background   { 0xff050505 };   // --background
    inline const juce::Colour panel        { 0xff0b0a09 };   // --card, warmed a touch
    inline const juce::Colour panelDeep    { 0xff070605 };
    inline const juce::Colour border       { 0xff1c1a18 };   // --border
    inline const juce::Colour borderBright { 0xff2c2823 };

    inline const juce::Colour text         { 0xffebe8e0 };   // --foreground
    inline const juce::Colour textMuted    { 0xff8b867d };
    inline const juce::Colour textFaint    { 0xff56514a };

    inline const juce::Colour gold         { 0xffc9a052 };   // brand gold
    inline const juce::Colour goldLight    { 0xffdca55e };   // --gold-light
    inline const juce::Colour goldDeep     { 0xff7d6335 };

    //==========================================================================
    /** Montserrat Bold — headings and control captions, as on the site. */
    juce::Font display (float height);

    /** Montserrat Medium — small labels. */
    juce::Font label (float height);

    /** Inter Medium — numeric readouts. */
    juce::Font value (float height);

    //==========================================================================
    /** The ZS Records waveform mark, as a path inside the given bounds. */
    juce::Path waveformMark (juce::Rectangle<float> bounds);

    /** One period-repeating brand wave, used for the background texture. */
    juce::Path brandWave (float width, float height, float period, float phase);

    /** The site's faint triangle overlay. */
    void paintTriangleTexture (juce::Graphics& g, juce::Rectangle<int> area, float opacity);
}
