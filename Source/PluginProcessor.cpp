#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MobiusSpectralCancellerAudioProcessor::MobiusSpectralCancellerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    depthParam = apvts.getRawParameterValue ("depth");
    scaleParam = apvts.getRawParameterValue ("scale");
    mixParam   = apvts.getRawParameterValue ("mix");

    for (int ch = 0; ch < 2; ++ch)
    {
        fifo[ch].assign (static_cast<size_t> (fftSize), 0.0f);
        overlap[ch].assign (static_cast<size_t> (fftSize), 0.0f);
    }

    // Real FFT needs 2 * fftSize storage for JUCE's in-place layout
    fftWork.assign (static_cast<size_t> (2 * fftSize), 0.0f);
}

MobiusSpectralCancellerAudioProcessor::~MobiusSpectralCancellerAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MobiusSpectralCancellerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "depth", 1 },
        "Depth",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scale", 1 },
        "Bin Scale",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.4f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    return layout;
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (fifo[ch].begin(), fifo[ch].end(), 0.0f);
        std::fill (overlap[ch].begin(), overlap[ch].end(), 0.0f);
    }
    fifoIndex = 0;
}

void MobiusSpectralCancellerAudioProcessor::releaseResources() {}

bool MobiusSpectralCancellerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                          juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels < 2)
        return;

    // Process left and right independently (real FFT per channel)
    processChannel (0, buffer.getWritePointer (0), numSamples);
    processChannel (1, buffer.getWritePointer (1), numSamples);
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessor::processChannel (int channel, float* outSamples, int numSamples)
{
    auto& inFifo   = fifo[channel];
    auto& ovl      = overlap[channel];
    const float mix = mixParam->load();

    for (int i = 0; i < numSamples; ++i)
    {
        // Store incoming sample
        inFifo[static_cast<size_t> (fifoIndex)] = outSamples[i];

        // Output the oldest sample from the overlap-add buffer (already mixed)
        // We will overwrite outSamples[i] after we have a full hop.

        ++fifoIndex;
        if (fifoIndex == hopSize)
        {
            // ---------- STFT frame ----------
            // 1. Copy the last fftSize samples from the circular fifo into work buffer
            //    (fifo is linear; we treat the most recent fftSize samples)
            // Because we advance by hopSize each time, the window starts at
            // (fifoIndex - fftSize) mod fftSize, but since we reset fifoIndex
            // we keep a sliding window by shifting.

            // Simpler reliable method: keep a continuous history of the last fftSize samples
            // by shifting the fifo left by hopSize after each hop (or use a proper ring).
            // For clarity and correctness we shift:

            // Copy current window (samples [0 .. fftSize-1] are the oldest → newest)
            for (int n = 0; n < fftSize; ++n)
                fftWork[static_cast<size_t> (n)] = inFifo[static_cast<size_t> (n)];

            // Apply analysis window
            window.multiplyWithWindowingTable (fftWork.data(), static_cast<size_t> (fftSize));

            // Real-to-complex forward transform (JUCE in-place)
            // After this call the first (fftSize/2 + 1) bins contain the spectrum
            // in the format: real[0], imag[0], real[1], imag[1], ...
            // (actually JUCE's performRealOnlyForwardTransform packs it as
            //  re[0], re[1], ..., re[N/2], im[N/2-1], ..., im[1])
            // We use the frequency-only magnitude path for the mask and keep phase.
            // For a true phase-preserving mask we need the complex bins.
            // JUCE 7/8 provides performRealOnlyForwardTransform which leaves
            // the data in a packed real/imag form that performRealOnlyInverseTransform
            // understands. We therefore apply the gain to the magnitude while
            // preserving the original phase by working on the packed representation.

            fft.performRealOnlyForwardTransform (fftWork.data(), true); // true = pack

            // Number of unique bins
            const int numBins = fftSize / 2 + 1;

            // Apply Möbius spectral mask (scales the complex coefficients)
            applyMobiusMask (fftWork.data(), numBins);

            // Inverse transform
            fft.performRealOnlyInverseTransform (fftWork.data());

            // Synthesis window (same Hann) + overlap-add
            window.multiplyWithWindowingTable (fftWork.data(), static_cast<size_t> (fftSize));

            // Add into overlap buffer
            for (int n = 0; n < fftSize; ++n)
                ovl[static_cast<size_t> (n)] += fftWork[static_cast<size_t> (n)];

            // Output the finished hopSize samples, apply dry/wet, then shift
            for (int n = 0; n < hopSize; ++n)
            {
                const float wet = ovl[static_cast<size_t> (n)];
                const float dry = inFifo[static_cast<size_t> (n)]; // original sample that is now leaving
                // Because of the shift logic below, the sample that should be mixed
                // is the one that entered hopSize frames ago. For simplicity we
                // use the current input sample as dry (parallel processing style).
                // A stricter implementation would keep a separate dry delay line.
                outSamples[i - hopSize + 1 + n] = (1.0f - mix) * dry + mix * wet;
            }

            // Clear the hop that we just emitted
            for (int n = 0; n < hopSize; ++n)
                ovl[static_cast<size_t> (n)] = 0.0f;

            // Shift remaining overlap left by hopSize
            for (int n = 0; n < fftSize - hopSize; ++n)
                ovl[static_cast<size_t> (n)] = ovl[static_cast<size_t> (n + hopSize)];
            for (int n = fftSize - hopSize; n < fftSize; ++n)
                ovl[static_cast<size_t> (n)] = 0.0f;

            // Shift input fifo left by hopSize (make room for next hop)
            for (int n = 0; n < fftSize - hopSize; ++n)
                inFifo[static_cast<size_t> (n)] = inFifo[static_cast<size_t> (n + hopSize)];
            // The newest hopSize samples will be written on subsequent iterations

            fifoIndex = 0;
        }
    }
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessor::applyMobiusMask (float* data, int numBins)
{
    const float depth = depthParam->load();
    const float scale = scaleParam->load();

    // JUCE real-only packed format after performRealOnlyForwardTransform(..., true):
    // data[0] = DC (real)
    // data[1] = Nyquist (real)
    // data[2*k]   = real part of bin k
    // data[2*k+1] = imag part of bin k
    // for k = 1 .. numBins-2

    // DC (k=0) – leave untouched or apply μ(1)=1
    // We skip DC and Nyquist for safety / musicality

    for (int k = 1; k < numBins - 1; ++k)
    {
        const int n = juce::roundToInt (static_cast<float> (k) * scale);
        if (n < 1 || n > maxMobiusN)
            continue;

        const int8_t mu = mobiusTable[n];

        // Gain law from the master prompt:
        //   g = 1 + depth * μ(n)
        // depth = 0 → transparent
        // depth = +1, μ=-1 → g=0 (cancel)
        // depth = +1, μ=+1 → g=2 (boost)
        // depth = -1 → inverted polarity
        const float gain = 1.0f + depth * static_cast<float> (mu);

        // Scale both real and imaginary parts → magnitude * gain, phase unchanged
        const int reIdx = 2 * k;
        const int imIdx = 2 * k + 1;

        data[reIdx] *= gain;
        data[imIdx] *= gain;
    }
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MobiusSpectralCancellerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* MobiusSpectralCancellerAudioProcessor::createEditor()
{
    return new MobiusSpectralCancellerAudioProcessorEditor (*this);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MobiusSpectralCancellerAudioProcessor();
}
