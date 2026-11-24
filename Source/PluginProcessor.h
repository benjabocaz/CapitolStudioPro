#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/AnalogModules.h"
#include "DSP/AcousticEngine.h"

class CapitolStudioProProcessor : public juce::AudioProcessor
{
public:
    CapitolStudioProProcessor();
    ~CapitolStudioProProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadCustomIR (const juce::File& file) { acousticEngine.loadCustomImpulseResponse (file); }
    juce::String getLoadedIRName() const { return acousticEngine.getLoadedIRName(); }

    float getOutputVuDb() const { return outputVuDb.load(); }
    float getCompGrDb() const { return compressor.getGainReductionDb(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateFilters();
    void processSourceAndMonitors (juce::AudioBuffer<float>& buffer);
    void processMicLocker (juce::AudioBuffer<float>& buffer);
    void processConsoleEQ (juce::AudioBuffer<float>& buffer);
    void processChamberStage (juce::AudioBuffer<float>& buffer);
    void processCompressorStage (juce::AudioBuffer<float>& buffer);

    double currentSampleRate = 44100.0;

    // Etapas DSP
    StereoBiquad srcHpf, srcLpf, srcMid;
    StereoBiquad micLow, micPres, micAir;
    StereoBiquad chHpf, chLpf;
    StereoBiquad eqHpf, eqLow, eqMid, eqHigh;

    OversampledAnalogStage analogStage;
    VintageCompressor compressor;
    AcousticEngine acousticEngine;

    juce::AudioBuffer<float> chamberScratch;
    std::atomic<float> outputVuDb { -60.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapitolStudioProProcessor)
};
