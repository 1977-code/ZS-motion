#include "BrandBackground.h"

namespace zs
{

using namespace juce;

namespace
{
    constexpr float headerWavePeriod = 190.0f;
    constexpr int   headerWaveTop    = 10;
    constexpr int   headerWaveHeight = 44;
}

BrandBackground::BrandBackground (String versionText)
    : version (std::move (versionText))
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
    startTimerHz (20);
}

void BrandBackground::resized()
{
    renderStaticLayer();
}

void BrandBackground::timerCallback()
{
    wavePhase -= 0.34f;

    if (wavePhase <= -headerWavePeriod)
        wavePhase += headerWavePeriod;

    repaint (0, headerWaveTop, getWidth(), headerWaveHeight);
}

//==============================================================================
void BrandBackground::renderStaticLayer()
{
    using namespace layout;

    const auto w = jmax (1, getWidth());
    const auto h = jmax (1, getHeight());

    staticLayer = Image (Image::ARGB, w, h, true);
    Graphics g (staticLayer);

    const auto bounds = Rectangle<int> (0, 0, w, h);

    //--- base ------------------------------------------------------------------
    g.setColour (theme::background);
    g.fillAll();

    //--- gold glow under the rotor, as on the site's hero ----------------------
    {
        const auto rotor  = rotorBounds().toFloat();
        const auto centre = rotor.getCentre();
        const auto radius = rotor.getWidth() * 0.85f;

        g.setGradientFill (ColourGradient (theme::gold.withAlpha (0.06f), centre,
                                           Colours::transparentBlack,
                                           centre.translated (radius, 0.0f), true));
        g.fillRect (bounds);
    }

    //--- triangle texture ------------------------------------------------------
    theme::paintTriangleTexture (g, bounds, 0.032f);

    //--- still brand waves, low in the frame ----------------------------------
    {
        const float opacities[] { 0.050f, 0.036f, 0.026f };
        const float periods[]   { 260.0f, 190.0f, 320.0f };
        const float tops[]      { 0.60f,  0.78f,  0.90f  };

        for (int i = 0; i < 3; ++i)
        {
            const auto strip = Rectangle<float> (0.0f, (float) h * tops[i], (float) w, 70.0f);
            auto wave = theme::brandWave (strip.getWidth(), strip.getHeight(), periods[i],
                                          (float) i * 55.0f);
            wave.applyTransform (AffineTransform::translation (0.0f, strip.getY()));

            g.setColour (theme::gold.withAlpha (opacities[i]));
            g.strokePath (wave, PathStrokeType (i == 0 ? 1.4f : 1.0f, PathStrokeType::curved,
                                                PathStrokeType::rounded));
        }
    }

    //--- header ---------------------------------------------------------------
    {
        const auto markArea = Rectangle<float> ((float) margin, 21.0f, 40.0f, 20.0f);
        g.setColour (theme::gold);
        g.strokePath (theme::waveformMark (markArea),
                      PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));

        auto title = theme::display (21.0f);
        title.setExtraKerningFactor (0.02f);
        g.setFont (title);
        g.setColour (theme::text);
        g.drawText ("ZS-motion", (int) markArea.getRight() + 14, 19, 300, 24,
                    Justification::centredLeft, false);

        auto tag = theme::label (10.0f);
        tag.setExtraKerningFactor (0.34f);
        g.setFont (tag);
        g.setColour (theme::textFaint);
        g.drawText ("KINETIC MODULATION", (int) markArea.getRight() + 14, 41, 300, 12,
                    Justification::centredLeft, false);

        auto brand = theme::display (11.0f);
        brand.setExtraKerningFactor (0.42f);
        g.setFont (brand);
        g.setColour (theme::gold.withAlpha (0.85f));
        g.drawText ("ZS RECORDS", w - margin - 240, 27, 240, 14,
                    Justification::centredRight, false);
    }

    //--- hairlines, each with the site's short gold lead-in --------------------
    const auto hairline = [&g, w] (int y, float alpha)
    {
        g.setColour (theme::border);
        g.fillRect (margin, y, w - 2 * margin, 1);

        g.setColour (theme::gold.withAlpha (alpha));
        g.fillRect (margin, y, 46, 1);
    };

    hairline (headerHeight, 0.75f);
    hairline (footerLine,   0.35f);

    //--- the selector panel beside the rotor -----------------------------------
    // Drawn here, with the rest of the still art, because the background sits over
    // the whole panel and would otherwise cover anything the parent painted.
    {
        const auto panel = panelBounds().toFloat();

        g.setColour (theme::panel);
        g.fillRoundedRectangle (panel, 8.0f);
        g.setColour (theme::border);
        g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

        auto rowLabel = theme::display (10.0f);
        rowLabel.setExtraKerningFactor (0.30f);
        g.setFont (rowLabel);

        const char* rows[] { "WAVE", "SYNC", "FAN", "DRIVE" };

        for (int i = 0; i < 4; ++i)
        {
            const auto row = panelRow (i);

            g.setColour (theme::textMuted);
            g.drawText (rows[i], row.withWidth (panelLabelWidth),
                        Justification::centredLeft, false);

            if (i < 3)
            {
                g.setColour (theme::border.withAlpha (0.5f));
                g.fillRect (row.getX(), row.getBottom() + 7, row.getWidth(), 1);
            }
        }
    }

    //--- block captions over the two knob rows --------------------------------
    {
        auto blockCaption = theme::display (10.0f);
        blockCaption.setExtraKerningFactor (0.32f);
        g.setFont (blockCaption);

        struct { int y; const char* text; Colour colour; } blocks[] {
            { captionOneY, "MOTION",     theme::textMuted },
            { captionTwoY, "SHAPE / FAN", theme::textFaint },
        };

        for (auto& b : blocks)
        {
            g.setColour (b.colour);
            g.drawText (b.text, margin, b.y, 200, 12, Justification::centredLeft, false);

            // A rule running out from the caption to the right edge.
            g.setColour (theme::border.withAlpha (0.55f));
            g.fillRect (margin + 116, b.y + 6, w - margin - (margin + 116), 1);
        }
    }

    //--- footer ---------------------------------------------------------------
    {
        auto foot = theme::label (10.0f);
        foot.setExtraKerningFactor (0.22f);
        g.setFont (foot);

        g.setColour (theme::textFaint);
        g.drawText (String::fromUTF8 ("ZS RECORDS \xc2\xb7 \xd0\xa1\xd0\xb8\xd0\xbc\xd1\x84\xd0\xb5\xd1\x80\xd0\xbe\xd0\xbf\xd0\xbe\xd0\xbb\xd1\x8c"),
                    margin, footerLine + 8, 400, 14, Justification::centredLeft, false);

        g.drawText (version, w - margin - 200, footerLine + 8, 200, 14,
                    Justification::centredRight, false);
    }
}

//==============================================================================
void BrandBackground::paint (Graphics& g)
{
    if (staticLayer.isValid())
        g.drawImageAt (staticLayer, 0, 0);
    else
        g.fillAll (theme::background);

    // The one moving element: the site's drifting waveform, behind the header.
    const auto strip = Rectangle<float> (0.0f, (float) headerWaveTop,
                                         (float) getWidth(), (float) headerWaveHeight);

    Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (strip.toNearestInt());

    auto wave = theme::brandWave (strip.getWidth(), strip.getHeight(), headerWavePeriod, wavePhase);
    wave.applyTransform (AffineTransform::translation (0.0f, strip.getY()));

    g.setColour (theme::gold.withAlpha (0.075f));
    g.strokePath (wave, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
}

} // namespace zs
