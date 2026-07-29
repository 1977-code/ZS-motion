#include "Theme.h"
#include <BinaryData.h>

namespace zs::theme
{

namespace
{
    juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    const juce::Typeface::Ptr& displayTypeface()
    {
        static const juce::Typeface::Ptr t = loadTypeface (ZsAssets::MontserratBold_ttf,
                                                           ZsAssets::MontserratBold_ttfSize);
        return t;
    }

    const juce::Typeface::Ptr& labelTypeface()
    {
        static const juce::Typeface::Ptr t = loadTypeface (ZsAssets::MontserratMedium_ttf,
                                                           ZsAssets::MontserratMedium_ttfSize);
        return t;
    }

    const juce::Typeface::Ptr& valueTypeface()
    {
        static const juce::Typeface::Ptr t = loadTypeface (ZsAssets::InterMedium_ttf,
                                                           ZsAssets::InterMedium_ttfSize);
        return t;
    }
}

juce::Font display (float height)
{
    return juce::Font (juce::FontOptions (displayTypeface()).withHeight (height));
}

juce::Font label (float height)
{
    return juce::Font (juce::FontOptions (labelTypeface()).withHeight (height));
}

juce::Font value (float height)
{
    return juce::Font (juce::FontOptions (valueTypeface()).withHeight (height));
}

//==============================================================================
juce::Path waveformMark (juce::Rectangle<float> bounds)
{
    // Same curve as WaveformLogo on the site: viewBox 0 0 120 50.
    juce::Path p;
    p.startNewSubPath (2.0f, 6.0f);
    p.cubicTo (10.0f,  6.0f,  12.0f, 46.0f,  22.0f, 46.0f);
    p.cubicTo (32.0f, 46.0f,  34.0f,  6.0f,  42.0f,  6.0f);
    p.cubicTo (50.0f,  6.0f,  52.0f, 46.0f,  62.0f, 46.0f);
    p.cubicTo (72.0f, 46.0f,  74.0f,  6.0f,  82.0f,  6.0f);
    p.cubicTo (90.0f,  6.0f,  92.0f, 46.0f, 102.0f, 46.0f);
    p.cubicTo (112.0f, 46.0f, 114.0f,  6.0f, 118.0f, 6.0f);

    p.applyTransform (p.getTransformToScaleToFit (bounds, true, juce::Justification::centred));
    return p;
}

juce::Path brandWave (float width, float height, float period, float phase)
{
    // The hero's generateWavePath(): a chain of cubic humps, one per period.
    juce::Path p;

    const float top    = height * 0.12f;
    const float bottom = height * 0.88f;
    const float start  = -period + std::fmod (phase, period);

    p.startNewSubPath (start, bottom);

    for (float x = start; x < width + period; x += period)
    {
        p.cubicTo (x + period * 0.25f, bottom, x + period * 0.25f, top,    x + period * 0.5f, top);
        p.cubicTo (x + period * 0.75f, top,    x + period * 0.75f, bottom, x + period,        bottom);
    }

    return p;
}

void paintTriangleTexture (juce::Graphics& g, juce::Rectangle<int> area, float opacity)
{
    // 40 x 40 outlined triangle, exactly as the hero section's SVG data-URI overlay.
    constexpr float tile = 40.0f;

    juce::Path triangle;
    triangle.startNewSubPath (20.0f,  2.0f);
    triangle.lineTo          (38.0f, 36.0f);
    triangle.lineTo          ( 2.0f, 36.0f);
    triangle.closeSubPath();

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area);
    g.setColour (gold.withAlpha (opacity));

    for (float y = (float) area.getY(); y < (float) area.getBottom(); y += tile)
        for (float x = (float) area.getX(); x < (float) area.getRight(); x += tile)
            g.strokePath (triangle, juce::PathStrokeType (0.5f),
                          juce::AffineTransform::translation (x, y));
}

} // namespace zs::theme
