#include "PluginEditor.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

namespace
{
    // Интерфейс английский, как принято в плагинах; объяснения — на языке студии.
    const String rotorTip = String::fromUTF8 (
        "Ротор — это и есть модуляция: форма фигуры равна форме LFO, скорость вращения — Rate, "
        "размах лепестков — Depth, расхождение двух тел — Width и Stereo. "
        "Тяни вверх/вниз — Depth, влево/вправо — Rate, двойной клик — сброс.");

    const String modeTip = String::fromUTF8 (
        "Что именно делает LFO. Переключение бесшовное: движки перекрёстно "
        "затухают за 30 мс, поэтому щелчка не будет.");

    const String depthTip = String::fromUTF8 (
        "Глубина модуляции. Для хоруса и вибрато — размах ухода задержки, "
        "для тремоло — насколько глубоко проседает громкость, для ротора — размах панорамы и доплера.");

    const String rateTip = String::fromUTF8 (
        "Скорость LFO, 0.01–10 Гц. При включённом SYNC берётся из темпа хоста, "
        "и эта ручка не работает.");

    const String mixTip = String::fromUTF8 (
        "Баланс сухого и обработанного, equal-power. На 0 % плагин измеримо прозрачен. "
        "Ниже 100 % вибрато превращается в хорус — так и должно быть.");

    const String widthTip = String::fromUTF8 (
        "Ширина стерео по схеме M/S: 0 — моно, 1 — как есть, 2 — подчёркнутые боковые.");

    const String satTip = String::fromUTF8 (
        "Насыщение на входе в обработку. Character задаёт характер, Quality — "
        "оверсэмплинг 2×/4×, который убирает алиасинг на сильном драйве.");

    const String outputTip = String::fromUTF8 ("Выходной уровень, −24…+12 dB.");

    const String stereoTip = String::fromUTF8 (
        "Смещение фазы LFO между каналами. 90° — обычный «широкий» хорус, "
        "0° — оба канала движутся вместе.");

    const String phaseTip = String::fromUTF8 (
        "Общий сдвиг фазы LFO. Важен, когда несколько инстансов должны двигаться "
        "не в такт друг другу.");

    const String feedbackTip = String::fromUTF8 (
        "Обратная связь линии задержки хоруса. С высокочастотным затуханием, "
        "поэтому повторы темнеют, а не звенят. Работает только в CHORUS.");

    const String fanTip = String::fromUTF8 (
        "FAN — лопастная амплитудная модуляция: несущая вращается, лопасти "
        "проходят перед сигналом. Производитель оригинала советует держать примерно до 50 %.");

    const String syncTip = String::fromUTF8 (
        "Привязать LFO к темпу хоста. Фаза считается от позиции транспорта, "
        "поэтому движение не уплывает при остановке и перемотке.");

    const String presetTip = String::fromUTF8 ("Фабричные пресеты. Стрелки — предыдущий и следующий.");

    void styleCombo (ComboBox& box)
    {
        box.setColour (ComboBox::backgroundColourId, zs::theme::panelDeep);
        box.setColour (ComboBox::textColourId,       zs::theme::text);
        box.setColour (ComboBox::outlineColourId,    zs::theme::border);
        box.setColour (ComboBox::arrowColourId,      zs::theme::gold);
        box.setJustificationType (Justification::centredLeft);
    }
}

//==============================================================================
ZsMotionAudioProcessorEditor::ModeSelector::ModeSelector (RangedAudioParameter& parameter)
    : attachment (parameter, [this] (float v)
      {
          const int active = jlimit (0, buttons.size() - 1, (int) v);
          for (int i = 0; i < buttons.size(); ++i)
              buttons[i]->setToggleState (i == active, dontSendNotification);
      }, nullptr)
{
    const auto names = zs::params::modeChoices();

    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = new TextButton (names[i]);
        b->setClickingTogglesState (false);
        b->setTooltip (modeTip);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        buttons.add (b);
        addAndMakeVisible (b);
    }

    attachment.sendInitialUpdate();
}

void ZsMotionAudioProcessorEditor::ModeSelector::resized()
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
ZsMotionAudioProcessorEditor::LevelMeter::LevelMeter (ZsMotionAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void ZsMotionAudioProcessorEditor::LevelMeter::timerCallback()
{
    const float now = processor.getUiOutputLevel();

    level += (now > level ? 0.5f : 0.16f) * (now - level);
    peak = jmax (peak * 0.985f, level);

    repaint();
}

void ZsMotionAudioProcessorEditor::LevelMeter::paint (Graphics& g)
{
    auto area = getLocalBounds();

    auto caption = zs::theme::display (10.0f);
    caption.setExtraKerningFactor (0.32f);
    g.setFont (caption);
    g.setColour (zs::theme::textMuted);
    g.drawText ("LEVEL", area.removeFromTop (14), Justification::centred, false);

    // A slim vertical column, the same language as the knobs beside it.
    const auto track = area.reduced (area.getWidth() / 2 - 7, 12).toFloat();

    g.setColour (zs::theme::panelDeep);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (zs::theme::border);
    g.drawRoundedRectangle (track.reduced (0.5f), 3.0f, 1.0f);

    const auto norm = [] (float v) { return jlimit (0.0f, 1.0f, std::sqrt (jmax (0.0f, v))); };

    auto filled = track.reduced (2.0f);
    const float h = filled.getHeight() * norm (level);
    auto bar = filled.removeFromBottom (h);

    g.setGradientFill (ColourGradient (zs::theme::gold, bar.getCentreX(), bar.getBottom(),
                                       zs::theme::goldLight, bar.getCentreX(), track.getY(), false));
    g.fillRoundedRectangle (bar, 2.0f);

    // Peak hold.
    const float py = track.getBottom() - 2.0f - (track.getHeight() - 4.0f) * norm (peak);
    g.setColour (peak > 0.995f ? Colour (0xffd9534f) : zs::theme::goldLight);
    g.fillRect (track.getX() + 1.5f, py, track.getWidth() - 3.0f, 1.6f);
}

//==============================================================================
ZsMotionAudioProcessorEditor::Content::Content (ZsMotionAudioProcessor& p)
    : processor (p),
      background (String ("v") + JucePlugin_VersionString),
      rotor (p),
      modes (*p.apvts.getParameter (zs::params::mode)),
      meter (p),
      modeWatcher (*p.apvts.getParameter (zs::params::mode),
                   [this] (float) { refreshRelevance(); }, nullptr),
      fanWatcher (*p.apvts.getParameter (zs::params::fanEnabled),
                  [this] (float) { refreshRelevance(); }, nullptr)
{
    addAndMakeVisible (background);
    addAndMakeVisible (rotor);
    addAndMakeVisible (modes);
    addAndMakeVisible (meter);

    rotor.setTooltip (rotorTip);

    // ── Knobs, in the order of KnobIndex ────────────────────────────────────
    addKnob (zs::params::depth,          "Depth")     .getSlider().setTooltip (depthTip);
    addKnob (zs::params::rate,           "Rate")      .getSlider().setTooltip (rateTip);
    addKnob (zs::params::mix,            "Mix")       .getSlider().setTooltip (mixTip);
    addKnob (zs::params::width,          "Width")     .getSlider().setTooltip (widthTip);
    addKnob (zs::params::saturation,     "Saturation").getSlider().setTooltip (satTip);
    addKnob (zs::params::output,         "Output")    .getSlider().setTooltip (outputTip);

    addKnob (zs::params::stereoPhase,    "Stereo")    .getSlider().setTooltip (stereoTip);
    addKnob (zs::params::phase,          "Phase")     .getSlider().setTooltip (phaseTip);
    addKnob (zs::params::chorusFeedback, "Feedback")  .getSlider().setTooltip (feedbackTip);
    addKnob (zs::params::fanAmount,      "Fan Amt")   .getSlider().setTooltip (fanTip);
    addKnob (zs::params::fanRate,        "Fan Rate")  .getSlider().setTooltip (fanTip);
    addKnob (zs::params::fanBlades,      "Blades")    .getSlider().setTooltip (fanTip);
    addKnob (zs::params::fanResonance,   "Fan Res")   .getSlider().setTooltip (fanTip);

    // ── Selector panel ───────────────────────────────────────────────────────
    addCombo (waveBox,      waveAtt,      zs::params::waveform,     zs::params::waveformChoices());
    addCombo (divisionBox,  divisionAtt,  zs::params::syncDivision, zs::params::divisionChoices());
    addCombo (modifierBox,  modifierAtt,  zs::params::syncModifier, zs::params::modifierChoices());
    addCombo (fanShapeBox,  fanShapeAtt,  zs::params::fanShape,     zs::params::fanShapeChoices());
    addCombo (characterBox, characterAtt, zs::params::satCharacter, zs::params::characterChoices());
    addCombo (qualityBox,   qualityAtt,   zs::params::satQuality,   zs::params::qualityChoices());

    for (auto* b : { &syncButton, &fanButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }

    syncButton.setTooltip (syncTip);
    fanButton.setTooltip (fanTip);

    syncAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, zs::params::syncEnabled, syncButton);
    fanAtt  = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, zs::params::fanEnabled,  fanButton);

    // ── Presets ──────────────────────────────────────────────────────────────
    styleCombo (presetBox);
    presetBox.setJustificationType (Justification::centred);
    presetBox.setTooltip (presetTip);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != processor.presets.getCurrentIndex())
            processor.presets.load (idx);
    };
    addAndMakeVisible (presetBox);

    for (auto* b : { &prevPreset, &nextPreset, &savePreset, &deletePreset, &randomPreset })
    {
        b->setTooltip (presetTip);
        addAndMakeVisible (b);
    }

    randomPreset.setTooltip (String::fromUTF8 (
        "Случайный пресет — быстрый способ найти звук, который сам бы не выставил."));

    prevPreset.onClick   = [this] { processor.presets.selectPrevious(); syncPresetBox(); };
    nextPreset.onClick   = [this] { processor.presets.selectNext();     syncPresetBox(); };
    randomPreset.onClick = [this] { processor.presets.selectRandom();   syncPresetBox(); };
    savePreset.onClick   = [this] { askToSavePreset(); };
    deletePreset.onClick = [this] { askToDeletePreset(); };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (String::fromUTF8 (
        "Обход всей обработки. Это тот же байпас, что и кнопка хоста, "
        "и задержка при обходе не меняется, чтобы дорожка не съезжала."));
    addAndMakeVisible (bypassButton);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, zs::params::bypass, bypassButton);

    addAndMakeVisible (dirtyWatcher);

    rebuildPresetList();

    modeWatcher.sendInitialUpdate();
    fanWatcher.sendInitialUpdate();
}

//==============================================================================
zs::ZsKnob& ZsMotionAudioProcessorEditor::Content::addKnob (const String& paramID,
                                                           const String& caption)
{
    auto* k = new zs::ZsKnob (*processor.apvts.getParameter (paramID), caption);
    knobs.add (k);
    addAndMakeVisible (k);
    return *k;
}

void ZsMotionAudioProcessorEditor::Content::addCombo (ComboBox& box,
                                                      std::unique_ptr<APVTS::ComboBoxAttachment>& att,
                                                      const String& paramID,
                                                      const StringArray& items)
{
    box.addItemList (items, 1);
    styleCombo (box);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, paramID, box);
}

void ZsMotionAudioProcessorEditor::Content::syncPresetBox()
{
    presetBox.setSelectedId (processor.presets.getCurrentIndex() + 1, dontSendNotification);
    deletePreset.setEnabled (processor.presets.isUserPreset (processor.presets.getCurrentIndex()));
    dirtyWatcher.repaint();
}

/** Factory set and the user's own, under their own headings. */
void ZsMotionAudioProcessorEditor::Content::rebuildPresetList()
{
    presetBox.clear (dontSendNotification);

    auto* menu = presetBox.getRootMenu();
    int id = 1;

    presetBox.addSectionHeading ("FACTORY");
    for (const auto& name : processor.presets.getFactoryNames())
        presetBox.addItem (name, id++);

    const auto userNames = processor.presets.getUserNames();

    if (! userNames.isEmpty())
    {
        menu->addSeparator();
        presetBox.addSectionHeading ("USER");

        for (const auto& name : userNames)
            presetBox.addItem (name, id++);
    }

    syncPresetBox();
}

void ZsMotionAudioProcessorEditor::Content::askToSavePreset()
{
    nameWindow = std::make_unique<AlertWindow> (
        String::fromUTF8 ("Сохранить пресет"),
        String::fromUTF8 ("Имя пресета. Существующий с тем же именем будет заменён."),
        MessageBoxIconType::NoIcon);

    nameWindow->addTextEditor ("name", processor.presets.getCurrentName(), {});
    nameWindow->addButton (String::fromUTF8 ("Сохранить"), 1, KeyPress (KeyPress::returnKey));
    nameWindow->addButton (String::fromUTF8 ("Отмена"),    0, KeyPress (KeyPress::escapeKey));

    nameWindow->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && nameWindow != nullptr)
        {
            const auto name = nameWindow->getTextEditorContents ("name");

            if (processor.presets.saveUserPreset (name) >= 0)
                rebuildPresetList();
        }

        nameWindow.reset();
    }), false);
}

void ZsMotionAudioProcessorEditor::Content::askToDeletePreset()
{
    const int index = processor.presets.getCurrentIndex();

    if (! processor.presets.isUserPreset (index))
        return;

    const auto name = processor.presets.getCurrentName();

    NativeMessageBox::showOkCancelBox (
        MessageBoxIconType::NoIcon,
        String::fromUTF8 ("Удалить пресет"),
        String::fromUTF8 ("Удалить «") + name + String::fromUTF8 ("» безвозвратно?"),
        this,
        ModalCallbackFunction::create ([this, index] (int result)
        {
            if (result == 1 && processor.presets.deleteUserPreset (index))
                rebuildPresetList();
        }));
}

//==============================================================================
ZsMotionAudioProcessorEditor::Content::DirtyWatcher::DirtyWatcher (Content& c)
    : owner (c)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (6);
}

void ZsMotionAudioProcessorEditor::Content::DirtyWatcher::timerCallback()
{
    const bool now = owner.processor.presets.isDirty();

    if (now != wasDirty)
    {
        wasDirty = now;
        repaint();
    }
}

void ZsMotionAudioProcessorEditor::Content::DirtyWatcher::paint (Graphics& g)
{
    if (! owner.processor.presets.isDirty())
        return;

    // A single gold dot: the preset no longer matches what you are hearing.
    g.setColour (zs::theme::gold);
    g.fillEllipse (getLocalBounds().toFloat().withSizeKeepingCentre (5.0f, 5.0f));
}

/** Grey out what the current mode and the Fan switch make irrelevant. */
void ZsMotionAudioProcessorEditor::Content::refreshRelevance()
{
    if (knobs.size() <= kFanRes)
        return;

    const int  mode = jlimit (0, 3, (int) processor.apvts.getRawParameterValue (zs::params::mode)->load());
    const bool fan  = processor.apvts.getRawParameterValue (zs::params::fanEnabled)->load() > 0.5f;
    const bool rotary = mode == 3;

    knobs[kFeedback]->setRelevant (mode == 0);            // chorus only
    knobs[kStereo]  ->setRelevant (! rotary);             // rotary makes its own stereo
    knobs[kPhase]   ->setRelevant (! rotary);

    for (int i : { kFanAmount, kFanRate, kBlades, kFanRes })
        knobs[i]->setRelevant (fan);

    fanShapeBox.setEnabled (fan);
    waveBox.setEnabled (! rotary);                        // rotary is not LFO-shaped
}

//==============================================================================
void ZsMotionAudioProcessorEditor::Content::resized()
{
    using namespace zs::layout;

    background.setBounds (getLocalBounds());
    rotor.setBounds (rotorBounds());
    modes.setBounds (modeBarBounds());
    bypassButton.setBounds (bypassBounds().reduced (2, 5));

    // Preset bar:  <  [ name ]  >   Save  Del
    {
        auto bar = presetBarBounds();

        prevPreset.setBounds (bar.removeFromLeft (30).reduced (2, 5));
        bar.removeFromLeft (3);

        deletePreset.setBounds (bar.removeFromRight (44).reduced (2, 5));
        bar.removeFromRight (3);
        savePreset.setBounds (bar.removeFromRight (52).reduced (2, 5));
        bar.removeFromRight (3);
        randomPreset.setBounds (bar.removeFromRight (30).reduced (2, 5));
        bar.removeFromRight (10);

        nextPreset.setBounds (bar.removeFromRight (30).reduced (2, 5));
        bar.removeFromRight (3);

        dirtyWatcher.setBounds (bar.removeFromRight (12));
        presetBox.setBounds (bar.reduced (0, 6));
    }

    // Selector panel rows.
    {
        auto row = panelRow (0);
        row.removeFromLeft (panelLabelWidth);
        waveBox.setBounds (row.reduced (0, 8));

        row = panelRow (1);
        row.removeFromLeft (panelLabelWidth);
        syncButton.setBounds  (row.removeFromLeft (56).reduced (0, 6));
        row.removeFromLeft (8);
        divisionBox.setBounds (row.removeFromLeft (74).reduced (0, 8));
        row.removeFromLeft (6);
        modifierBox.setBounds (row.reduced (0, 8));

        row = panelRow (2);
        row.removeFromLeft (panelLabelWidth);
        fanButton.setBounds   (row.removeFromLeft (56).reduced (0, 6));
        row.removeFromLeft (8);
        fanShapeBox.setBounds (row.removeFromLeft (110).reduced (0, 8));

        row = panelRow (3);
        row.removeFromLeft (panelLabelWidth);
        characterBox.setBounds (row.removeFromLeft (118).reduced (0, 8));
        row.removeFromLeft (6);
        qualityBox.setBounds   (row.removeFromLeft (78).reduced (0, 8));
    }

    // Two rows of seven cells: six knobs plus the meter, then seven knobs.
    for (int i = 0; i < 6; ++i)
        knobs[i]->setBounds (knobCell (i, rowOneY));

    meter.setBounds (knobCell (6, rowOneY).reduced (10, 0));

    for (int i = 6; i < knobs.size(); ++i)
        knobs[i]->setBounds (knobCell (i - 6, rowTwoY));
}

//==============================================================================
ZsMotionAudioProcessorEditor::ZsMotionAudioProcessorEditor (ZsMotionAudioProcessor& p)
    : AudioProcessorEditor (&p), plugin (p), content (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    setResizable (true, true);

    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio ((double) zs::layout::width / (double) zs::layout::height);
        constrainer->setSizeLimits (roundToInt (zs::layout::width  * 0.72),
                                    roundToInt (zs::layout::height * 0.72),
                                    roundToInt (zs::layout::width  * 1.8),
                                    roundToInt (zs::layout::height * 1.8));
    }

    const auto storedWidth = (int) plugin.apvts.state.getProperty (
        ZsMotionAudioProcessor::editorWidthProperty, zs::layout::width);

    const auto startWidth = jlimit (roundToInt (zs::layout::width * 0.72),
                                    roundToInt (zs::layout::width * 1.8),
                                    storedWidth);

    setSize (startWidth,
             roundToInt ((double) startWidth * zs::layout::height / zs::layout::width));
}

ZsMotionAudioProcessorEditor::~ZsMotionAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ZsMotionAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (zs::theme::background);
}

void ZsMotionAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) zs::layout::width;

    content.setBounds (0, 0, zs::layout::width, zs::layout::height);
    content.setTransform (AffineTransform::scale (scale));

    plugin.apvts.state.setProperty (ZsMotionAudioProcessor::editorWidthProperty,
                                    getWidth(), nullptr);
}
