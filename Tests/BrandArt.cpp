/*
    Renders the brand artwork used by the app bundles and the installers.

    Built with -DZSMOTION_BUILD_ART=ON:

        ZSmotionArt <output-directory>

    Everything is drawn with the same zs::theme code the interface uses, so the
    icon and the installer graphics can never drift away from the plug-in itself —
    change the palette in one place and re-run this.

    Writes:
        icon.png            1024 square, the app / plug-in icon (JUCE turns this
                            into .icns and .ico for the bundles)
        installer.ico       multi-size icon for the Windows installer executable
        wizard-large.png    497 x 314, the Inno Setup wizard panel
        wizard-small.png    138 x 140, the Inno Setup header badge
        pkg-background.png  620 x 418, the macOS installer background

    The mark is the rotor: the five-lobed polar figure the plug-in draws, which is
    the product's own shape rather than a picture of a household fan.
*/
#include <juce_gui_extra/juce_gui_extra.h>

#include "gui/Theme.h"
#include "dsp/LFO.h"
#include "utils/MathUtils.h"

using namespace juce;

namespace
{
    /** The rotor, as a closed polar path — the same construction as RotorView. */
    Path rotorMark (Point<float> centre, float radius, float depth, int lobes, float rotation)
    {
        Path p;
        constexpr int steps = 480;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float v = zs::waveformShape (zs::Waveform::Sine,
                                               zs::math::wrap01 (t * (float) lobes));
            const float r = radius * (1.0f + depth * v);
            const float a = zs::math::twoPi * (t + rotation) - zs::math::halfPi;

            const Point<float> pt { centre.x + std::cos (a) * r,
                                    centre.y + std::sin (a) * r };

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        p.closeSubPath();
        return p;
    }

    void paintBackdrop (Graphics& g, Rectangle<float> area, Point<float> centre, float glow)
    {
        g.setColour (zs::theme::background);
        g.fillAll();

        g.setGradientFill (ColourGradient (zs::theme::gold.withAlpha (0.16f), centre,
                                           Colours::transparentBlack,
                                           centre.translated (glow, 0.0f), true));
        g.fillRect (area);

        zs::theme::paintTriangleTexture (g, area.toNearestInt(), 0.045f);
    }

    /** The rotor with its wake, centred in `area`. */
    void paintRotor (Graphics& g, Rectangle<float> area, float radius)
    {
        const auto centre = area.getCentre();

        // A short wake, so the mark reads as something in motion.
        for (int i = 5; i >= 1; --i)
        {
            const float age = (float) i / 6.0f;
            g.setColour (zs::theme::gold.withAlpha (0.13f * (1.0f - age)));
            g.strokePath (rotorMark (centre, radius * (1.0f - 0.16f * age), 0.34f, 5, -0.02f * (float) i),
                          PathStrokeType (radius * 0.030f));
        }

        const auto body = rotorMark (centre, radius, 0.34f, 5, 0.0f);

        g.setColour (zs::theme::gold.withAlpha (0.10f));
        g.fillPath (body);

        g.setColour (zs::theme::goldLight);
        g.strokePath (body, PathStrokeType (radius * 0.052f, PathStrokeType::curved,
                                            PathStrokeType::rounded));

        // The hub.
        g.setColour (zs::theme::background);
        g.fillEllipse (Rectangle<float> (radius * 0.34f, radius * 0.34f).withCentre (centre));
        g.setColour (zs::theme::gold);
        g.drawEllipse (Rectangle<float> (radius * 0.34f, radius * 0.34f).withCentre (centre),
                       radius * 0.035f);
        g.fillEllipse (Rectangle<float> (radius * 0.11f, radius * 0.11f).withCentre (centre));
    }

    void writePng (const Image& image, const File& file)
    {
        file.deleteFile();

        if (auto stream = std::unique_ptr<FileOutputStream> (file.createOutputStream()))
        {
            PNGImageFormat png;
            png.writeImageToStream (image, *stream);
        }

        std::printf ("  wrote %s\n", file.getFullPathName().toRawUTF8());
    }

    /** Encode a set of images as a Vista-style .ico with PNG-compressed entries. */
    void writeIco (const Image& source, const File& file, const Array<int>& sizes)
    {
        Array<MemoryBlock> encoded;

        for (int size : sizes)
        {
            const auto scaled = source.rescaled (size, size, Graphics::highResamplingQuality);

            MemoryBlock block;
            MemoryOutputStream stream (block, false);
            PNGImageFormat png;
            png.writeImageToStream (scaled, stream);
            stream.flush();
            encoded.add (block);
        }

        MemoryOutputStream out;

        // ICONDIR
        out.writeShort (0);                              // reserved
        out.writeShort (1);                              // type: icon
        out.writeShort ((short) sizes.size());           // image count

        int offset = 6 + 16 * sizes.size();

        for (int i = 0; i < sizes.size(); ++i)
        {
            const int size = sizes[i];

            out.writeByte ((char) (size >= 256 ? 0 : size));   // 0 means 256
            out.writeByte ((char) (size >= 256 ? 0 : size));
            out.writeByte (0);                                 // palette
            out.writeByte (0);                                 // reserved
            out.writeShort (1);                                // colour planes
            out.writeShort (32);                               // bits per pixel
            out.writeInt ((int) encoded[i].getSize());         // data size
            out.writeInt (offset);                             // data offset

            offset += (int) encoded[i].getSize();
        }

        for (auto& block : encoded)
            out.write (block.getData(), block.getSize());

        file.deleteFile();
        file.replaceWithData (out.getData(), out.getDataSize());

        std::printf ("  wrote %s (%d sizes)\n", file.getFullPathName().toRawUTF8(), sizes.size());
    }
}

//==============================================================================
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juceInit;

    const File outputDirectory = argc > 1
        ? File (String (argv[1])).getFullPathName()
        : File::getCurrentWorkingDirectory();

    outputDirectory.createDirectory();

    std::printf ("ZS-motion brand artwork\n");

    // ── The icon ────────────────────────────────────────────────────────────
    Image icon (Image::ARGB, 1024, 1024, true);
    {
        Graphics g (icon);
        const auto area = Rectangle<float> (1024.0f, 1024.0f);

        paintBackdrop (g, area, area.getCentre(), 620.0f);
        paintRotor (g, area, 296.0f);      // leaves margin so it reads when small

        // A hairline frame, the way the panels are edged.
        g.setColour (zs::theme::borderBright);
        g.drawRoundedRectangle (area.reduced (6.0f), 96.0f, 6.0f);
    }
    writePng (icon, outputDirectory.getChildFile ("icon.png"));
    writeIco (icon, outputDirectory.getChildFile ("installer.ico"),
              { 16, 24, 32, 48, 64, 128, 256 });

    // ── Inno Setup wizard panel ─────────────────────────────────────────────
    {
        Image wizard (Image::ARGB, 497, 314, true);
        Graphics g (wizard);
        const auto area = Rectangle<float> (497.0f, 314.0f);

        // The rotor sits low and to the right, leaving the top for the name.
        const auto centre = Point<float> (330.0f, 214.0f);
        paintBackdrop (g, area, centre, 340.0f);
        paintRotor (g, Rectangle<float> (150.0f, 64.0f, 360.0f, 300.0f), 118.0f);

        g.setColour (zs::theme::gold);
        g.strokePath (zs::theme::waveformMark ({ 34.0f, 34.0f, 96.0f, 40.0f }),
                      PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        auto title = zs::theme::display (34.0f);
        title.setExtraKerningFactor (0.02f);
        g.setFont (title);
        g.setColour (zs::theme::text);
        g.drawText ("ZS-motion", 34, 88, 420, 40, Justification::topLeft, false);

        auto tag = zs::theme::label (13.0f);
        tag.setExtraKerningFactor (0.34f);
        g.setFont (tag);
        g.setColour (zs::theme::textMuted);
        g.drawText ("KINETIC MODULATION", 34, 130, 420, 18, Justification::topLeft, false);

        g.setColour (zs::theme::border);
        g.fillRect (34, 158, 120, 1);
        g.setColour (zs::theme::gold.withAlpha (0.7f));
        g.fillRect (34, 158, 40, 1);

        writePng (wizard, outputDirectory.getChildFile ("wizard-large.png"));
    }

    // ── Inno Setup header badge ─────────────────────────────────────────────
    {
        Image badge (Image::ARGB, 138, 140, true);
        Graphics g (badge);
        const auto area = Rectangle<float> (138.0f, 140.0f);

        paintBackdrop (g, area, area.getCentre(), 120.0f);
        paintRotor (g, area, 46.0f);

        writePng (badge, outputDirectory.getChildFile ("wizard-small.png"));
    }

    // ── macOS installer background ──────────────────────────────────────────
    {
        Image background (Image::ARGB, 620, 418, true);
        Graphics g (background);
        const auto area = Rectangle<float> (620.0f, 418.0f);

        // Offset low-left, because the installer lays its text over the top-right.
        const auto centre = Point<float> (170.0f, 300.0f);
        paintBackdrop (g, area, centre, 420.0f);
        paintRotor (g, Rectangle<float> (30.0f, 160.0f, 280.0f, 280.0f), 96.0f);

        g.setColour (zs::theme::gold);
        g.strokePath (zs::theme::waveformMark ({ 32.0f, 34.0f, 86.0f, 36.0f }),
                      PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));

        auto brand = zs::theme::display (12.0f);
        brand.setExtraKerningFactor (0.42f);
        g.setFont (brand);
        g.setColour (zs::theme::gold.withAlpha (0.85f));
        g.drawText ("ZS RECORDS", 32, 80, 240, 16, Justification::topLeft, false);

        writePng (background, outputDirectory.getChildFile ("pkg-background.png"));
    }

    return 0;
}
