#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/ModulationEngine.h"

//==============================================================================
class ZsMotionAudioProcessor final : public juce::AudioProcessor
{
public:
    ZsMotionAudioProcessor();
    ~ZsMotionAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                             { return true; }

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
    zs::PresetManager presets { apvts };

    // UI taps.
    float getUiPhase()       const noexcept { return engine.getUiPhase(); }
    float getUiLfoValue()    const noexcept { return engine.getUiLfoValue(); }
    float getUiOutputLevel() const noexcept { return engine.getUiOutputLevel(); }
    float getUiFanPhase()    const noexcept { return engine.getUiFanPhase(); }
    float getUiHornPhase()   const noexcept { return engine.getUiHornPhase(); }
    float getUiDrumPhase()   const noexcept { return engine.getUiDrumPhase(); }

    /** The LFO rate the engine is actually running (sync already resolved). */
    float getEffectiveRateHz() const noexcept { return effectiveRate.load (std::memory_order_relaxed); }

    double getCurrentBpm()   const noexcept { return currentBpm.load (std::memory_order_relaxed); }

    /** Editor size, remembered inside the plug-in state. */
    static constexpr const char* editorWidthProperty  = "editorWidth";
    static constexpr const char* editorHeightProperty = "editorHeight";

private:
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }

    zs::ModulationEngine engine;

    // Cached parameter pointers.
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pSyncEnabled = nullptr;
    std::atomic<float>* pSyncDivision = nullptr;
    std::atomic<float>* pSyncModifier = nullptr;
    std::atomic<float>* pWaveform = nullptr;
    std::atomic<float>* pPhase = nullptr;
    std::atomic<float>* pStereoPhase = nullptr;
    std::atomic<float>* pSaturation = nullptr;
    std::atomic<float>* pSatCharacter = nullptr;
    std::atomic<float>* pSatQuality = nullptr;
    std::atomic<float>* pFanEnabled = nullptr;
    std::atomic<float>* pFanAmount = nullptr;
    std::atomic<float>* pFanRate = nullptr;
    std::atomic<float>* pFanBlades = nullptr;
    std::atomic<float>* pIrregularity = nullptr;
    std::atomic<float>* pFanShape = nullptr;
    std::atomic<float>* pFanResonance = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pChorusFeedback = nullptr;

    std::atomic<double> currentBpm { 120.0 };
    std::atomic<float>  effectiveRate { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZsMotionAudioProcessor)
};
