#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZsMotionAudioProcessor::ZsMotionAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZSMOTION", zs::params::createLayout())
{
    pMode           = raw (zs::params::mode);
    pDepth          = raw (zs::params::depth);
    pRate           = raw (zs::params::rate);
    pSyncEnabled    = raw (zs::params::syncEnabled);
    pSyncDivision   = raw (zs::params::syncDivision);
    pSyncModifier   = raw (zs::params::syncModifier);
    pWaveform       = raw (zs::params::waveform);
    pPhase          = raw (zs::params::phase);
    pStereoPhase    = raw (zs::params::stereoPhase);
    pSaturation     = raw (zs::params::saturation);
    pSatCharacter   = raw (zs::params::satCharacter);
    pSatQuality     = raw (zs::params::satQuality);
    pFanEnabled     = raw (zs::params::fanEnabled);
    pFanAmount      = raw (zs::params::fanAmount);
    pFanRate        = raw (zs::params::fanRate);
    pFanBlades      = raw (zs::params::fanBlades);
    pIrregularity   = raw (zs::params::irregularity);
    pFanShape       = raw (zs::params::fanShape);
    pFanResonance   = raw (zs::params::fanResonance);
    pWidth          = raw (zs::params::width);
    pMix            = raw (zs::params::mix);
    pOutput         = raw (zs::params::output);
    pChorusFeedback = raw (zs::params::chorusFeedback);
}

ZsMotionAudioProcessor::~ZsMotionAudioProcessor() = default;

//==============================================================================
void ZsMotionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Hand the engine its settings first, so the oversampling latency is settled
    // before the first block and the host hears about it straight away.
    engine.setSettings (gatherSettings (120.0, 0.0, false));
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());

    reportedLatency = engine.getLatencySamples();
    pendingLatency.store (reportedLatency, std::memory_order_relaxed);
    setLatencySamples (reportedLatency);
}

void ZsMotionAudioProcessor::handleAsyncUpdate()
{
    const int latency = pendingLatency.load (std::memory_order_relaxed);

    if (latency != getLatencySamples())
        setLatencySamples (latency);
}

void ZsMotionAudioProcessor::releaseResources()
{
    engine.reset();
}

bool ZsMotionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
zs::ModulationEngine::Settings
ZsMotionAudioProcessor::gatherSettings (double bpm, double ppq, bool playing) const
{
    zs::ModulationEngine::Settings s;

    s.mode           = (zs::ModulationEngine::Mode) juce::jlimit (0, 3, (int) pMode->load());
    s.depth          = pDepth->load();
    s.waveform       = (int) pWaveform->load();
    s.phaseDeg       = pPhase->load();
    s.stereoPhaseDeg = pStereoPhase->load();

    const bool sync       = pSyncEnabled->load() > 0.5f;
    const int  division   = (int) pSyncDivision->load();
    const int  modifier   = (int) pSyncModifier->load();

    if (sync)
    {
        s.rateHz = zs::params::syncedFrequency (bpm, division, modifier);

        if (playing)
        {
            const float periodQuarters = zs::params::divisionInQuarters (division)
                                       * zs::params::modifierMultiplier (modifier);
            if (periodQuarters > 0.0f)
            {
                float ph01 = (float) (ppq / (double) periodQuarters);
                ph01 -= std::floor (ph01);
                s.hostPhase01 = ph01;
            }
        }
    }
    else
    {
        s.rateHz = pRate->load();
    }

    effectiveRate.store (s.rateHz, std::memory_order_relaxed);

    s.fanEnabled     = pFanEnabled->load() > 0.5f;
    s.fanRateHz      = pFanRate->load();
    s.fanAmount      = pFanAmount->load();
    s.fanBlades      = pFanBlades->load();
    s.irregularity   = pIrregularity->load();
    s.fanShape       = (int) pFanShape->load();
    s.fanResonance   = pFanResonance->load();

    s.saturation     = pSaturation->load();
    s.satCharacter   = (int) pSatCharacter->load();
    s.satQuality     = (int) pSatQuality->load();

    s.width          = pWidth->load();
    s.mix            = pMix->load();
    s.outputDb       = pOutput->load();
    s.chorusFeedback = pChorusFeedback->load();

    return s;
}

void ZsMotionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ── Transport / tempo ────────────────────────────────────────────────────
    double bpm = 120.0, ppq = 0.0;
    bool   playing = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    }

    currentBpm.store (bpm, std::memory_order_relaxed);

    engine.setSettings (gatherSettings (bpm, ppq, playing));
    engine.process (buffer);

    // Changing the oversampling changes the latency; hand that off to the message
    // thread rather than calling into the host from here.
    const int latency = engine.getLatencySamples();

    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        pendingLatency.store (latency, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
}

//==============================================================================
juce::AudioProcessorEditor* ZsMotionAudioProcessor::createEditor()
{
    return new ZsMotionAudioProcessorEditor (*this);
}

void ZsMotionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();

    // Stamped so a future version can recognise and migrate an older session
    // instead of silently mis-reading it.
    tree.setProperty (schemaVersionProperty, currentSchemaVersion, nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZsMotionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    // Sessions saved before the stamp existed read as 0; nothing has needed
    // migrating yet, so they load as they are. New shapes get handled here.
    const int version = (int) tree.getProperty (schemaVersionProperty, 0);

    if (version > currentSchemaVersion)
    {
        // Saved by a newer build. Load what we understand rather than refusing —
        // unknown parameters are simply absent from this build's layout.
        jassert (version <= currentSchemaVersion);
    }

    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZsMotionAudioProcessor();
}
