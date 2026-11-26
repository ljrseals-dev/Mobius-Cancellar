#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

MobiusSpectralCancellerAudioProcessor::MobiusSpectralCancellerAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    depthParam = apvts.getRawParameterValue ("depth");
    scaleParam = apvts.getRawParameterValue ("scale");
    mixParam = apvts.getRawParameterValue ("mix");

    windowValues.resize (static_cast<size_t> (fftSize));
    for (int n = 0; n < fftSize; ++n)
        windowValues[static_cast<size_t> (n)] =
            0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                  * static_cast<float> (n) / static_cast<float> (fftSize - 1));

    for (int i = 0; i < fftSize; ++i)
    {
        unsigned reversed = 0;
        unsigned value = static_cast<unsigned> (i);
        for (int bit = 0; bit < fftOrder; ++bit)
        {
            reversed = (reversed << 1u) | (value & 1u);
            value >>= 1u;
        }
        bitReverse[static_cast<size_t> (i)] = reversed;
    }

    for (int k = 0; k < fftSize / 2; ++k)
    {
        const float phase = -juce::MathConstants<float>::twoPi
                          * static_cast<float> (k) / static_cast<float> (fftSize);
        fftTwiddles[static_cast<size_t> (k)] = { std::cos (phase), std::sin (phase) };
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        inputRing[static_cast<size_t> (ch)].assign (fftSize, 0.0f);
        olaAccum[static_cast<size_t> (ch)].assign (ringCapacity, 0.0f);
        olaWeight[static_cast<size_t> (ch)].assign (ringCapacity, 0.0f);
        dryDelay[static_cast<size_t> (ch)].assign (ringCapacity, 0.0f);
    }
}

MobiusSpectralCancellerAudioProcessor::~MobiusSpectralCancellerAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
MobiusSpectralCancellerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "depth", 1 }, "Depth",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scale", 1 }, "Bin Scale",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.4f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    return layout;
}

float MobiusSpectralCancellerAudioProcessor::computeMobiusGain (int8_t mu, float depth) noexcept
{
    return 1.0f + juce::jlimit (-1.0f, 1.0f, depth)
                    * static_cast<float> (juce::jlimit<int> (-1, 1, static_cast<int> (mu)));
}

void MobiusSpectralCancellerAudioProcessor::prepareToPlay (double, int)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (inputRing[ch].begin(), inputRing[ch].end(), 0.0f);
        std::fill (olaAccum[ch].begin(), olaAccum[ch].end(), 0.0f);
        std::fill (olaWeight[ch].begin(), olaWeight[ch].end(), 0.0f);
        std::fill (dryDelay[ch].begin(), dryDelay[ch].end(), 0.0f);
        sampleCounters[ch] = 0;
    }
    fftWork.fill ({ 0.0f, 0.0f });
    setLatencySamples (latencySamples);
}

void MobiusSpectralCancellerAudioProcessor::releaseResources() {}

bool MobiusSpectralCancellerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MobiusSpectralCancellerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2)
        return;

    processChannel (0, buffer.getWritePointer (0), buffer.getNumSamples());
    processChannel (1, buffer.getWritePointer (1), buffer.getNumSamples());
}

int MobiusSpectralCancellerAudioProcessor::ringIndex (int64_t sample) noexcept
{
    return static_cast<int> (sample % ringCapacity);
}

void MobiusSpectralCancellerAudioProcessor::processChannel (int channel,
                                                            float* samples,
                                                            int numSamples)
{
    auto& history = inputRing[static_cast<size_t> (channel)];
    auto& dry = dryDelay[static_cast<size_t> (channel)];
    auto& accumulated = olaAccum[static_cast<size_t> (channel)];
    auto& weights = olaWeight[static_cast<size_t> (channel)];
    auto& counter = sampleCounters[static_cast<size_t> (channel)];
    const float mix = mixParam->load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const int64_t current = counter;
        const float input = samples[i];

        history[static_cast<size_t> (current % fftSize)] = input;
        dry[static_cast<size_t> (ringIndex (current))] = input;

        if ((current + 1) % hopSize == 0)
            processFrame (channel, current);

        if (current >= latencySamples)
        {
            const int64_t outputSample = current - latencySamples;
            const int slot = ringIndex (outputSample);
            const float norm = weights[static_cast<size_t> (slot)];
            const float wet = norm > 1.0e-12f
                ? accumulated[static_cast<size_t> (slot)] / norm : 0.0f;
            const float delayedDry = dry[static_cast<size_t> (slot)];

            samples[i] = delayedDry * (1.0f - mix) + wet * mix;
            accumulated[static_cast<size_t> (slot)] = 0.0f;
            weights[static_cast<size_t> (slot)] = 0.0f;
        }
        else
            samples[i] = 0.0f;

        ++counter;
    }
}

void MobiusSpectralCancellerAudioProcessor::performFFT (bool inverse) noexcept
{
    for (int i = 0; i < fftSize; ++i)
    {
        const unsigned j = bitReverse[static_cast<size_t> (i)];
        if (static_cast<unsigned> (i) < j)
            std::swap (fftWork[static_cast<size_t> (i)], fftWork[static_cast<size_t> (j)]);
    }

    for (int length = 2; length <= fftSize; length <<= 1)
    {
        const int half = length >> 1;
        const int twiddleStep = fftSize / length;
        for (int base = 0; base < fftSize; base += length)
        {
            for (int j = 0; j < half; ++j)
            {
                auto twiddle = fftTwiddles[static_cast<size_t> (j * twiddleStep)];
                if (inverse)
                    twiddle = std::conj (twiddle);

                const auto even = fftWork[static_cast<size_t> (base + j)];
                const auto odd = fftWork[static_cast<size_t> (base + j + half)] * twiddle;
                fftWork[static_cast<size_t> (base + j)] = even + odd;
                fftWork[static_cast<size_t> (base + j + half)] = even - odd;
            }
        }
    }

    if (inverse)
    {
        constexpr float inverseSize = 1.0f / static_cast<float> (fftSize);
        for (auto& value : fftWork)
            value *= inverseSize;
    }
}

void MobiusSpectralCancellerAudioProcessor::processFrame (int channel, int64_t frameEndSample)
{
    auto& history = inputRing[static_cast<size_t> (channel)];
    auto& accumulated = olaAccum[static_cast<size_t> (channel)];
    auto& weights = olaWeight[static_cast<size_t> (channel)];
    const int64_t frameStart = frameEndSample - fftSize + 1;

    for (int n = 0; n < fftSize; ++n)
    {
        const int64_t sourceSample = frameStart + n;
        const float input = sourceSample >= 0
            ? history[static_cast<size_t> (sourceSample % fftSize)] : 0.0f;
        fftWork[static_cast<size_t> (n)] = {
            input * windowValues[static_cast<size_t> (n)], 0.0f
        };
    }

    performFFT (false);
    applyMobiusMask();

    // Rebuild the negative-frequency half explicitly to preserve a real
    // inverse transform after applying the positive-frequency mask.
    for (int k = 1; k < fftSize / 2; ++k)
        fftWork[static_cast<size_t> (fftSize - k)] =
            std::conj (fftWork[static_cast<size_t> (k)]);

    performFFT (true);

    for (int n = 0; n < fftSize; ++n)
    {
        const int64_t destinationSample = frameStart + n;
        if (destinationSample < 0)
            continue;

        const int slot = ringIndex (destinationSample);
        const float w = windowValues[static_cast<size_t> (n)];
        accumulated[static_cast<size_t> (slot)] += fftWork[static_cast<size_t> (n)].real() * w;
        weights[static_cast<size_t> (slot)] += w * w;
    }
}

void MobiusSpectralCancellerAudioProcessor::applyMobiusMask() noexcept
{
    const float depth = depthParam->load (std::memory_order_relaxed);
    const float scale = scaleParam->load (std::memory_order_relaxed);

    // DC (k=0) and Nyquist (k=N/2) are intentionally excluded.
    // A real, non-negative scalar gain preserves phase.
    for (int k = 1; k < fftSize / 2; ++k)
    {
        const int n = juce::roundToInt (static_cast<float> (k) * scale);
        if (n < 1 || n > maxMobiusN)
            continue;

        const float gain = computeMobiusGain (mobiusTable[n], depth);
        fftWork[static_cast<size_t> (k)] *= gain;
    }
}

void MobiusSpectralCancellerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void MobiusSpectralCancellerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MobiusSpectralCancellerAudioProcessor::createEditor()
{
    return new MobiusSpectralCancellerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MobiusSpectralCancellerAudioProcessor();
}
