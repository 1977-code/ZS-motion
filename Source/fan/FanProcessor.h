#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "FanParameters.h"
#include "FanEngine.h"

//==============================================================================
/**
    ZS-MOTION-FAN — the short-reach sibling of ZS-motion.

    Same house, same code base, opposite intent: ten controls instead of
    twenty-four, a genuine ring-modulating fan instead of the sibling's blade
    amplitude modulation, a tight 5.6 ms chorus instead of a wide 11/15 ms one, and
    no oversampling so there is no latency to report.
*/
class ZsFanAudioProcessor final : public juce::AudioProcessor
{
public:
    ZsFanAudioProcessor();
    ~ZsFanAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                             { return true; }

    juce::AudioParameterBool* getBypassParameter() const override
    {
        return dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (zs::fanparams::bypass));
    }

    const juce::String getName() const override                 { return JucePlugin_Name; }
    bool acceptsMidi() const override                           { return false; }
    bool producesMidi() const override                          { return false; }
    bool isMidiEffect() const override                          { return false; }
    double getTailLengthSeconds() const override                { return 0.0; }

    int getNumPrograms() override                               { return 1; }
    int getCurrentProgram() override                            { return 0; }
    void setCurrentProgram (int) override                       {}
    const juce::String getProgramName (int) override            { return "Default"; }
    void changeProgramName (int, const juce::String&) override  {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    float getUiPhase()    const noexcept { return engine.getUiPhase(); }
    float getUiFanPhase() const noexcept { return engine.getUiFanPhase(); }
    float getUiLevel()    const noexcept { return engine.getUiLevel(); }

    /** The rate the engine is running, with sync already worked out. */
    float getEffectiveRateHz() const noexcept { return effectiveRate.load (std::memory_order_relaxed); }

    static constexpr const char* editorWidthProperty   = "editorWidth";
    static constexpr const char* schemaVersionProperty = "schemaVersion";
    static constexpr int         currentSchemaVersion  = 1;

private:
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }

    zs::fan::FanEngine::Settings gatherSettings (double bpm, double ppq, bool playing) const;

    zs::fan::FanEngine engine;

    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pActive = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pRateHz = nullptr;
    std::atomic<float>* pSyncOn = nullptr;
    std::atomic<float>* pRateSync = nullptr;
    std::atomic<float>* pModifier = nullptr;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pSaturation = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pFanOn = nullptr;
    std::atomic<float>* pMix = nullptr;

    mutable std::atomic<float> effectiveRate { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZsFanAudioProcessor)
};
