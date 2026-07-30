#include "FanEditor.h"
#include "RingFan.h"
#include "MorphLFO.h"

using namespace juce;
using namespace zs::fanlayout;
using APVTS = AudioProcessorValueTreeState;

namespace
{
    const String depthTip = String::fromUTF8 (
        "Глубина. Для хоруса и вибрато — размах ухода задержки, для тремоло — "
        "просадка громкости, для ротора — размах панорамы и доплера. При включённом "
        "FAN она же задаёт амплитуду несущей: на нуле вентилятор выдаёт тишину, "
        "а не сухой сигнал.");

    const String rateTip = String::fromUTF8 (
        "Скорость LFO, 0–32 Гц линейно. Она же — частота несущей вентилятора: "
        "боковые полосы стоят гребёнкой ровно с этим шагом.");

    const String morphTip = String::fromUTF8 (
        "Форма LFO без ступеней: синус → треугольник → пила → меандр. "
        "Промежуточные положения — настоящие, а не округление к ближайшему.");

    const String satTip   = String::fromUTF8 ("Насыщение на входе в обработку.");
    const String widthTip = String::fromUTF8 ("Ширина стерео по схеме M/S: 0 — моно, 1 — как есть, 2 — подчёркнутые боковые.");
    const String mixTip   = String::fromUTF8 (
        "Баланс сухого и обработанного. С включённым вентилятором держи примерно "
        "до половины — кольцевая модуляция быстро съедает разборчивость.");

    const String fanTip = String::fromUTF8 (
        "Вентилятор: кольцевая модуляция биполярной лопастной несущей. "
        "Тон на выходе вырезается начисто — этим она и отличается от амплитудной.");

    const String activeTip = String::fromUTF8 (
        "Убирает модуляцию, не трогая вентилятор — можно слушать одни лопасти.");

    void styleCombo (ComboBox& box)
    {
        box.setColour (ComboBox::backgroundColourId, zs::theme::panelDeep);
        box.setColour (ComboBox::textColourId,       zs::theme::text);
        box.setColour (ComboBox::outlineColourId,    zs::theme::border);
        box.setColour (ComboBox::arrowColourId,      zs::theme::gold);
    }
}

//==============================================================================
ZsFanAudioProcessorEditor::CarrierView::CarrierView (ZsFanAudioProcessor& p)
    : processor (p)
{
    startTimerHz (40);
}

void ZsFanAudioProcessorEditor::CarrierView::timerCallback()
{
    sweep = processor.getUiPhase();
    level += 0.3f * (processor.getUiLevel() - level);
    repaint();
}

void ZsFanAudioProcessorEditor::CarrierView::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (zs::theme::panelDeep);
    g.fillRoundedRectangle (area, 7.0f);

    const auto plot = area.reduced (14.0f, 12.0f);
    const float midY = plot.getCentreY();
    const float amp  = plot.getHeight() * 0.36f;

    // Zero line, and the rails the carrier actually reaches.
    g.setColour (zs::theme::border.withAlpha (0.6f));
    g.fillRect (plot.getX(), midY, plot.getWidth(), 1.0f);

    const bool fanOn = processor.apvts.getRawParameterValue (zs::fanparams::fanOn)->load() > 0.5f;
    const float morph = processor.apvts.getRawParameterValue (zs::fanparams::morph)->load();

    constexpr int cycles = 2;
    const int steps = jmax (96, (int) plot.getWidth());

    // The morphing LFO, behind.
    {
        zs::fan::MorphLFO shape;
        shape.prepare (48000.0);
        shape.setMorph (morph);

        Path p;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            shape.setPhase (zs::math::wrap01 (t * cycles));
            const float v = shape.value();

            const Point<float> pt { plot.getX() + plot.getWidth() * t, midY - v * amp * 0.75f };

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        g.setColour (zs::theme::goldLight.withAlpha (0.42f));
        g.strokePath (p, PathStrokeType (1.3f));
    }

    // The blade carrier, in front — only meaningful when the fan is running. Drawn
    // through RingFan's own shape function, so this is the curve the audio thread
    // multiplies by, band-limiting and all, rather than an idealised rectangle.
    {
        Path p;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float phase = zs::math::wrap01 (t * cycles);
            const float carrier = zs::fan::RingFan::shapeAt (phase);

            const Point<float> pt { plot.getX() + plot.getWidth() * t, midY - carrier * amp };

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        g.setColour (fanOn ? zs::theme::gold : zs::theme::textFaint.withAlpha (0.35f));
        g.strokePath (p, PathStrokeType (fanOn ? 1.9f : 1.1f));
    }

    // Where the blade is right now.
    {
        const float x = plot.getX() + plot.getWidth() * (sweep / (float) cycles);

        g.setColour (zs::theme::gold.withAlpha (0.35f));
        g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
    }

    // Captions.
    auto caption = zs::theme::display (9.0f);
    caption.setExtraKerningFactor (0.3f);
    g.setFont (caption);

    g.setColour (fanOn ? zs::theme::gold : zs::theme::textFaint);
    g.drawText (fanOn ? "RING CARRIER - BIPOLAR" : "CARRIER OFF",
                area.reduced (14.0f, 8.0f), Justification::topLeft, false);

    g.setColour (zs::theme::textFaint);
    g.drawText (String (processor.getEffectiveRateHz(), 2) + " Hz",
                area.reduced (14.0f, 8.0f), Justification::topRight, false);

    g.setColour (zs::theme::border);
    g.drawRoundedRectangle (area.reduced (0.5f), 7.0f, 1.0f);
}

//==============================================================================
ZsFanAudioProcessorEditor::ModeRow::ModeRow (RangedAudioParameter& parameter)
    : attachment (parameter, [this] (float v)
      {
          const int active = jlimit (0, buttons.size() - 1, (int) v);

          for (int i = 0; i < buttons.size(); ++i)
              buttons[i]->setToggleState (i == active, dontSendNotification);
      }, nullptr)
{
    const auto names = zs::fanparams::modeChoices();

    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = new TextButton (names[i]);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        buttons.add (b);
        addAndMakeVisible (b);
    }

    attachment.sendInitialUpdate();
}

void ZsFanAudioProcessorEditor::ModeRow::resized()
{
    auto area = getLocalBounds();
    const int n = buttons.size();

    for (int i = 0; i < n; ++i)
    {
        const int x0 = area.getX() + roundToInt ((double) area.getWidth() * i / n);
        const int x1 = area.getX() + roundToInt ((double) area.getWidth() * (i + 1) / n);
        buttons[i]->setBounds (Rectangle<int> (x0, area.getY(), x1 - x0, area.getHeight()).reduced (3, 4));
    }
}

//==============================================================================
ZsFanAudioProcessorEditor::Content::Content (ZsFanAudioProcessor& p)
    : processor (p),
      carrier (p),
      modes (*p.apvts.getParameter (zs::fanparams::mode))
{
    addAndMakeVisible (carrier);
    addAndMakeVisible (modes);

    // Deliberately not `using namespace zs::fanparams` here — it has a `width`
    // and so does the layout namespace above.
    namespace ids = zs::fanparams;

    addKnob (ids::depth,      "Depth");
    addKnob (ids::rateHz,     "Rate");
    addKnob (ids::morph,      "Morph");
    addKnob (ids::saturation, "Saturation");
    addKnob (ids::width,      "Width");
    addKnob (ids::mix,        "Mix");

    knobs[0]->getSlider().setTooltip (depthTip);
    knobs[1]->getSlider().setTooltip (rateTip);
    knobs[2]->getSlider().setTooltip (morphTip);
    knobs[3]->getSlider().setTooltip (satTip);
    knobs[4]->getSlider().setTooltip (widthTip);
    knobs[5]->getSlider().setTooltip (mixTip);

    addToggle (activeButton, activeAtt, ids::active, activeTip);
    addToggle (fanButton,    fanAtt,    ids::fanOn,  fanTip);
    addToggle (syncButton,   syncAtt,   ids::syncOn, String::fromUTF8 ("Привязать LFO к темпу хоста."));
    addToggle (bypassButton, bypassAtt, ids::bypass, String::fromUTF8 ("Обход всей обработки, он же байпас хоста."));

    addCombo (divisionBox, divisionAtt, ids::rateSync, ids::divisionChoices());
    addCombo (modifierBox, modifierAtt, ids::modifier, ids::modifierChoices());
}

void ZsFanAudioProcessorEditor::Content::addKnob (const String& paramID, const String& caption)
{
    auto* k = new zs::ZsKnob (*processor.apvts.getParameter (paramID), caption);
    knobs.add (k);
    addAndMakeVisible (k);
}

void ZsFanAudioProcessorEditor::Content::addToggle (TextButton& button,
                                                   std::unique_ptr<APVTS::ButtonAttachment>& att,
                                                   const String& paramID, const String& tip)
{
    button.setClickingTogglesState (true);
    button.setTooltip (tip);
    addAndMakeVisible (button);
    att = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, paramID, button);
}

void ZsFanAudioProcessorEditor::Content::addCombo (ComboBox& box,
                                                  std::unique_ptr<APVTS::ComboBoxAttachment>& att,
                                                  const String& paramID, const StringArray& items)
{
    box.addItemList (items, 1);
    styleCombo (box);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, paramID, box);
}

void ZsFanAudioProcessorEditor::Content::paint (Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll (zs::theme::background);
    zs::theme::paintTriangleTexture (g, bounds, 0.03f);

    // A gold wash behind the carrier strip.
    {
        const auto centre = Point<float> ((float) width * 0.5f, 168.0f);
        g.setGradientFill (ColourGradient (zs::theme::gold.withAlpha (0.05f), centre,
                                           Colours::transparentBlack,
                                           centre.translated ((float) width * 0.55f, 0.0f), true));
        g.fillRect (bounds);
    }

    // Header.
    {
        const auto markArea = Rectangle<float> ((float) margin, 18.0f, 36.0f, 18.0f);
        g.setColour (zs::theme::gold);
        g.strokePath (zs::theme::waveformMark (markArea),
                      PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        auto title = zs::theme::display (19.0f);
        title.setExtraKerningFactor (0.02f);
        g.setFont (title);
        g.setColour (zs::theme::text);
        g.drawText ("ZS-MOTION-FAN", (int) markArea.getRight() + 12, 16, 320, 22,
                    Justification::centredLeft, false);

        auto tag = zs::theme::label (9.0f);
        tag.setExtraKerningFactor (0.32f);
        g.setFont (tag);
        g.setColour (zs::theme::textFaint);
        g.drawText ("RING MODULATION", (int) markArea.getRight() + 12, 37, 320, 12,
                    Justification::centredLeft, false);

        auto brand = zs::theme::display (10.0f);
        brand.setExtraKerningFactor (0.4f);
        g.setFont (brand);
        g.setColour (zs::theme::gold.withAlpha (0.85f));
        g.drawText ("ZS RECORDS", width - margin - 200, 22, 200, 14,
                    Justification::centredRight, false);
    }

    // Hairline under the header, with the house gold lead-in.
    g.setColour (zs::theme::border);
    g.fillRect (margin, 58, width - 2 * margin, 1);
    g.setColour (zs::theme::gold.withAlpha (0.7f));
    g.fillRect (margin, 58, 40, 1);

    // Footer.
    auto foot = zs::theme::label (9.0f);
    foot.setExtraKerningFactor (0.2f);
    g.setFont (foot);
    g.setColour (zs::theme::textFaint);
    g.drawText (String::fromUTF8 ("ZS RECORDS \xc2\xb7 \xd0\xa1\xd0\xb8\xd0\xbc\xd1\x84\xd0\xb5\xd1\x80\xd0\xbe\xd0\xbf\xd0\xbe\xd0\xbb\xd1\x8c"),
                margin, height - 26, 300, 14, Justification::centredLeft, false);

    g.drawText (String ("v") + JucePlugin_VersionString,
                width - margin - 200, height - 26, 200, 14, Justification::centredRight, false);
}

void ZsFanAudioProcessorEditor::Content::resized()
{
    // Mode row and the switches that sit beside it.
    {
        auto row = Rectangle<int> (margin, 70, width - 2 * margin, 32);

        bypassButton.setBounds (row.removeFromRight (76).reduced (2, 3));
        row.removeFromRight (8);
        modes.setBounds (row.removeFromLeft (jmin (392, row.getWidth())));
    }

    carrier.setBounds (margin, 112, width - 2 * margin, 112);

    // Switch strip under the carrier.
    {
        auto row = Rectangle<int> (margin, 234, width - 2 * margin, 28);

        activeButton.setBounds (row.removeFromLeft (76).reduced (2, 2));
        row.removeFromLeft (6);
        fanButton.setBounds (row.removeFromLeft (60).reduced (2, 2));
        row.removeFromLeft (18);
        syncButton.setBounds (row.removeFromLeft (64).reduced (2, 2));
        row.removeFromLeft (6);
        divisionBox.setBounds (row.removeFromLeft (72).reduced (2, 2));
        row.removeFromLeft (5);
        modifierBox.setBounds (row.removeFromLeft (88).reduced (2, 2));
    }

    // Six knobs, one row.
    {
        const int usable = width - 2 * margin;
        const int y = 282;
        const int h = 96;

        for (int i = 0; i < knobs.size(); ++i)
        {
            const int x0 = margin + roundToInt ((double) usable * i / knobs.size());
            const int x1 = margin + roundToInt ((double) usable * (i + 1) / knobs.size());
            knobs[i]->setBounds (x0, y, x1 - x0, h);
        }
    }
}

//==============================================================================
ZsFanAudioProcessorEditor::ZsFanAudioProcessorEditor (ZsFanAudioProcessor& p)
    : AudioProcessorEditor (&p), plugin (p), content (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    setResizable (true, true);

    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio ((double) width / (double) height);
        constrainer->setSizeLimits (roundToInt (width * 0.75), roundToInt (height * 0.75),
                                    roundToInt (width * 1.7),  roundToInt (height * 1.7));
    }

    const auto stored = (int) plugin.apvts.state.getProperty (
        ZsFanAudioProcessor::editorWidthProperty, width);

    const auto startWidth = jlimit (roundToInt (width * 0.75), roundToInt (width * 1.7), stored);

    setSize (startWidth, roundToInt ((double) startWidth * height / width));
}

ZsFanAudioProcessorEditor::~ZsFanAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ZsFanAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (zs::theme::background);
}

void ZsFanAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) width;

    content.setBounds (0, 0, width, height);
    content.setTransform (AffineTransform::scale (scale));

    plugin.apvts.state.setProperty (ZsFanAudioProcessor::editorWidthProperty, getWidth(), nullptr);
}
