#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include "MobiusTable.h"

//==============================================================================
class MobiusSpectralCancellerAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MobiusSpectralCancellerAudioProcessor();
    ~MobiusSpectralCancellerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Möbius Spectral Canceller"; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Expose for editor visualisation
    const MobiusTable& getMobiusTable() const { return mobiusTable; }
    int getFftSize() const noexcept { return fftSize; }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // FFT / STFT
    static constexpr int fftOrder   = 11;                 // 2^11 = 2048
    static constexpr int fftSize    = 1 << fftOrder;
    static constexpr int hopSize    = fftSize / 4;        // 75 % overlap
    static constexpr int maxMobiusN = 16384;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { static_cast<size_t> (fftSize),
                                                 juce::dsp::WindowingFunction<float>::hann };

    MobiusTable mobiusTable { maxMobiusN };

    // Per-channel circular input / overlap-add buffers
    std::array<std::vector<float>, 2> fifo;
    std::array<std::vector<float>, 2> overlap;
    int fifoIndex = 0;

    // Working memory (real FFT layout: first fftSize/2+1 bins are usable)
    std::vector<float> fftWork;

    // Parameters (atomic for real-time safety)
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* mixParam   = nullptr;

    //==============================================================================
    void processChannel (int channel, float* outSamples, int numSamples);
    void applyMobiusMask (float* magnitudeSpectrum, int numBins);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobiusSpectralCancellerAudioProcessor)
};
