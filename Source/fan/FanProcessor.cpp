#include "FanProcessor.h"
#include "FanEditor.h"

using namespace zs::fanparams;

//==============================================================================
ZsFanAudioProcessor::ZsFanAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZSMOTIONFAN", createLayout())
{
    pBypass     = raw (bypass);
    pActive     = raw (active);
    pMode       = raw (mode);
    pDepth      = raw (depth);
    pRateHz     = raw (rateHz);
    pSyncOn     = raw (syncOn);
    pRateSync   = raw (rateSync);
    pModifier   = raw (modifier);
    pMorph      = raw (morph);
    pSaturation = raw (saturation);
    pWidth      = raw (width);
    pFanOn      = raw (fanOn);
    pMix        = raw (mix);
}

ZsFanAudioProcessor::~ZsFanAudioProcessor() = default;

//==============================================================================
void ZsFanAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setSettings (gatherSettings (120.0, 0.0, false));
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples (engine.getLatencySamples());
}

void ZsFanAudioProcessor::releaseResources()
{
    engine.reset();
}

bool ZsFanAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
zs::fan::FanEngine::Settings
ZsFanAudioProcessor::gatherSettings (double bpm, double ppq, bool playing) const
{
    zs::fan::FanEngine::Settings s;

    s.bypassed   = pBypass->load() > 0.5f;
    s.active     = pActive->load() > 0.5f;
    s.mode       = (zs::fan::FanEngine::Mode) juce::jlimit (0, 3, (int) pMode->load());
    s.depth      = pDepth->load();
    s.morph      = pMorph->load();
    s.saturation = pSaturation->load();
    s.width      = pWidth->load();
    s.fanOn      = pFanOn->load() > 0.5f;
    s.mix        = pMix->load();

    if (pSyncOn->load() > 0.5f)
    {
        const int division = (int) pRateSync->load();
        const int mod      = (int) pModifier->load();

        s.rateHz = zs::params::syncedFrequency (bpm, division, mod);

        if (playing)
        {
            const float periodQuarters = zs::params::divisionInQuarters (division)
                                       * zs::params::modifierMultiplier (mod);

            if (periodQuarters > 0.0f)
            {
                float phase = (float) (ppq / (double) periodQuarters);
                phase -= std::floor (phase);
                s.hostPhase01 = phase;
            }
        }
    }
    else
    {
        s.rateHz = pRateHz->load();
    }

    effectiveRate.store (s.rateHz, std::memory_order_relaxed);

    return s;
}

void ZsFanAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    double bpm = 120.0, ppq = 0.0;
    bool   playing = false;

    if (auto* head = getPlayHead())
    {
        if (auto position = head->getPosition())
        {
            if (auto b = position->getBpm())         bpm = *b;
            if (auto q = position->getPpqPosition()) ppq = *q;
            playing = position->getIsPlaying();
        }
    }

    engine.setSettings (gatherSettings (bpm, ppq, playing));
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* ZsFanAudioProcessor::createEditor()
{
    return new ZsFanAudioProcessorEditor (*this);
}

void ZsFanAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();
    tree.setProperty (schemaVersionProperty, currentSchemaVersion, nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZsFanAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (tree.isValid())
        apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZsFanAudioProcessor();
}
