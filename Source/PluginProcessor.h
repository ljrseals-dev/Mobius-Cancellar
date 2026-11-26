#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>
#include "MobiusTable.h"

class MobiusSpectralCancellerAudioProcessor : public juce::AudioProcessor
{
public:
    MobiusSpectralCancellerAudioProcessor();
    ~MobiusSpectralCancellerAudioProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Möbius Spectral Canceller"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const MobiusTable& getMobiusTable() const { return mobiusTable; }
    int getFftSize() const noexcept { return fftSize; }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int maxMobiusN = 16384;
    static constexpr int latencySamples = 2 * fftSize;
    static constexpr int ringCapacity = 4 * fftSize;
    juce::dsp::FFT fft { fftOrder };
    MobiusTable mobiusTable { maxMobiusN };
    std::vector<float> windowValues;
    std::array<std::vector<float>, 2> inputRing;
    std::array<std::vector<float>, 2> olaAccum;
    std::array<std::vector<float>, 2> olaWeight;
    std::array<std::vector<float>, 2> dryDelay;
    std::array<int64_t, 2> sampleCounters {{ 0, 0 }};
    std::vector<float> fftWork;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    void processChannel (int channel, float* samples, int numSamples);
    void processFrame (int channel, int64_t frameEndSample);
    void applyMobiusMask (float* packedSpectrum, int numBins);
    static int ringIndex (int64_t sample) noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobiusSpectralCancellerAudioProcessor)
};
