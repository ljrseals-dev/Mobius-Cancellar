#include "../Source/PluginProcessor.h"
#include <juce_events/juce_events.h>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr int latency = 2048;
constexpr float tolerance = 3.0e-4f;

bool runIdentityTest (int blockSize, float mix, const char* label)
{
    constexpr int totalSamples = 24000;
    MobiusSpectralCancellerAudioProcessor processor;
    processor.getAPVTS().getParameter ("depth")->setValueNotifyingHost (0.5f); // normalized 0.5 => depth 0
    processor.getAPVTS().getParameter ("mix")->setValueNotifyingHost (mix);
    processor.prepareToPlay (48000.0, blockSize);

    if (processor.getLatencySamples() != latency)
    {
        std::cerr << "reported latency mismatch: " << processor.getLatencySamples() << "\n";
        return false;
    }

    std::vector<float> left (totalSamples), right (totalSamples);
    for (int i = 0; i < totalSamples; ++i)
    {
        left[static_cast<size_t> (i)] = 0.31f * std::sin (0.013f * i)
                                      + 0.17f * std::sin (0.071f * i);
        right[static_cast<size_t> (i)] = 0.23f * std::cos (0.019f * i)
                                       - 0.11f * std::sin (0.053f * i);
    }

    juce::MidiBuffer midi;
    float maxError = 0.0f;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const int count = juce::jmin (blockSize, totalSamples - offset);
        juce::AudioBuffer<float> block (2, count);
        for (int i = 0; i < count; ++i)
        {
            block.setSample (0, i, left[static_cast<size_t> (offset + i)]);
            block.setSample (1, i, right[static_cast<size_t> (offset + i)]);
        }

        processor.processBlock (block, midi);
        for (int i = 0; i < count; ++i)
        {
            const int64_t absolute = static_cast<int64_t> (offset + i);
            const float expectedL = absolute >= latency
                ? left[static_cast<size_t> (absolute - latency)] : 0.0f;
            const float expectedR = absolute >= latency
                ? right[static_cast<size_t> (absolute - latency)] : 0.0f;
            maxError = juce::jmax (maxError, std::abs (block.getSample (0, i) - expectedL));
            maxError = juce::jmax (maxError, std::abs (block.getSample (1, i) - expectedR));
        }
    }

    if (maxError > tolerance)
    {
        std::cerr << label << " blockSize=" << blockSize
                  << " maxError=" << maxError << " tolerance=" << tolerance << "\n";
        return false;
    }
    return true;
}

bool testMobiusTableAndGain()
{
    const MobiusTable table (128);
    const bool tableOk = table[1] == 1 && table[2] == -1 && table[4] == 0
                      && table[6] == 1 && table[30] == -1 && table[12] == 0;
    const bool gainOk = std::abs (MobiusSpectralCancellerAudioProcessor::computeMobiusGain (-1, 1.0f)) < 1.0e-7f
                     && std::abs (MobiusSpectralCancellerAudioProcessor::computeMobiusGain (0, 1.0f) - 1.0f) < 1.0e-7f
                     && std::abs (MobiusSpectralCancellerAudioProcessor::computeMobiusGain (1, 1.0f) - 2.0f) < 1.0e-7f
                     && std::abs (MobiusSpectralCancellerAudioProcessor::computeMobiusGain (1, -1.0f)) < 1.0e-7f;
    return tableOk && gainOk;
}

bool testStateRoundTrip()
{
    MobiusSpectralCancellerAudioProcessor source;
    source.getAPVTS().getParameter ("depth")->setValueNotifyingHost (0.75f);
    source.getAPVTS().getParameter ("scale")->setValueNotifyingHost (0.35f);
    source.getAPVTS().getParameter ("mix")->setValueNotifyingHost (0.60f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    MobiusSpectralCancellerAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto* a = source.getAPVTS().getRawParameterValue ("depth");
    const auto* b = restored.getAPVTS().getRawParameterValue ("depth");
    const auto* c = source.getAPVTS().getRawParameterValue ("scale");
    const auto* d = restored.getAPVTS().getRawParameterValue ("scale");
    const auto* e = source.getAPVTS().getRawParameterValue ("mix");
    const auto* f = restored.getAPVTS().getRawParameterValue ("mix");
    return std::abs (a->load() - b->load()) < 1.0e-5f
        && std::abs (c->load() - d->load()) < 1.0e-5f
        && std::abs (e->load() - f->load()) < 1.0e-5f;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    bool passed = testMobiusTableAndGain() && testStateRoundTrip();

    for (const int blockSize : { 1, 64, 127, 128, 256, 512, 1024 })
    {
        passed = runIdentityTest (blockSize, 1.0f, "fully-wet zero-depth identity") && passed;
        passed = runIdentityTest (blockSize, 0.0f, "delayed dry path") && passed;
    }

    if (! passed)
        return 1;

    std::cout << "MSC table, gain, state, identity, and dry-path tests passed.\n";
    return 0;
}
