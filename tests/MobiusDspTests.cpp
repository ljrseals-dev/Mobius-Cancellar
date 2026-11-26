#include "../Source/PluginProcessor.h"
#include <juce_events/juce_events.h>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr int latency = 4096;

bool runIdentityTest (int blockSize, float mix, const char* label)
{
    constexpr int totalSamples = 24000;
    MobiusSpectralCancellerAudioProcessor processor;
    processor.getAPVTS().getParameter ("depth")->setValueNotifyingHost (0.5f); // depth = 0
    processor.getAPVTS().getParameter ("mix")->setValueNotifyingHost (mix);
    processor.prepareToPlay (48000.0, blockSize);

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
                ? left[static_cast<size_t> (absolute - latency)] * mix : 0.0f;
            const float expectedR = absolute >= latency
                ? right[static_cast<size_t> (absolute - latency)] * mix : 0.0f;
            maxError = juce::jmax (maxError, std::abs (block.getSample (0, i) - expectedL));
            maxError = juce::jmax (maxError, std::abs (block.getSample (1, i) - expectedR));
        }
    }

    const float tolerance = 2.0e-4f;
    if (maxError > tolerance)
    {
        std::cerr << label << " blockSize=" << blockSize
                  << " maxError=" << maxError << " tolerance=" << tolerance << "\n";
        return false;
    }
    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    bool passed = true;
    for (const int blockSize : { 64, 128, 256, 512, 1024 })
    {
        passed &= runIdentityTest (blockSize, 1.0f, "fully-wet identity");
        passed &= runIdentityTest (blockSize, 0.0f, "delayed dry");
    }

    if (! passed)
        return 1;

    std::cout << "MSC DSP identity and delayed-dry tests passed.\n";
    return 0;
}
