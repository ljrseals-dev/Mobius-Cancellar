#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MobiusSpectralCancellerAudioProcessorEditor::MobiusSpectralCancellerAudioProcessorEditor (
    MobiusSpectralCancellerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (520, 340);

    // Depth
    depthSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    depthLabel.setText ("Depth", juce::dontSendNotification);
    depthLabel.setJustificationType (juce::Justification::centred);
    depthAttachment = std::make_unique<SliderAttachment> (processor.getAPVTS(), "depth", depthSlider);

    // Scale
    scaleSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    scaleSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    scaleLabel.setText ("Bin Scale", juce::dontSendNotification);
    scaleLabel.setJustificationType (juce::Justification::centred);
    scaleAttachment = std::make_unique<SliderAttachment> (processor.getAPVTS(), "scale", scaleSlider);

    // Mix
    mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    mixLabel.setText ("Dry/Wet", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixAttachment = std::make_unique<SliderAttachment> (processor.getAPVTS(), "mix", mixSlider);

    addAndMakeVisible (depthSlider);
    addAndMakeVisible (scaleSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (depthLabel);
    addAndMakeVisible (scaleLabel);
    addAndMakeVisible (mixLabel);

    // Visual
    maskImage = juce::Image (juce::Image::RGB, 512, 64, true);
    startTimerHz (15);
}

MobiusSpectralCancellerAudioProcessorEditor::~MobiusSpectralCancellerAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void MobiusSpectralCancellerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1e));

    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("Möbius Spectral Canceller", getLocalBounds().removeFromTop (36),
                      juce::Justification::centred, 1);

    g.setFont (12.0f);
    g.setColour (juce::Colours::grey);
    g.drawFittedText ("Möbius-indexed spectral gain  ·  2048-point STFT  ·  75% overlap",
                      getLocalBounds().removeFromTop (56).removeFromBottom (20),
                      juce::Justification::centred, 1);

    // Mask visualisation
    if (maskImage.isValid())
    {
        auto area = getLocalBounds().reduced (20, 0).withTrimmedTop (70).withHeight (70);
        g.setColour (juce::Colours::black);
        g.fillRect (area);
        g.drawImage (maskImage, area.toFloat());
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawRect (area, 1);
        g.setFont (11.0f);
        g.drawText ("μ(n) mask (green = +1, red = −1, black = 0)", area.getX(), area.getBottom() + 4,
                    area.getWidth(), 16, juce::Justification::centred);
    }
}

void MobiusSpectralCancellerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (30).withTrimmedTop (160);

    const int knobW = 120;
    const int gap   = 30;
    int x = (getWidth() - (3 * knobW + 2 * gap)) / 2;

    depthSlider.setBounds (x, area.getY(), knobW, 110);
    depthLabel.setBounds  (x, area.getY() + 110, knobW, 20);
    x += knobW + gap;

    scaleSlider.setBounds (x, area.getY(), knobW, 110);
    scaleLabel.setBounds  (x, area.getY() + 110, knobW, 20);
    x += knobW + gap;

    mixSlider.setBounds (x, area.getY(), knobW, 110);
    mixLabel.setBounds  (x, area.getY() + 110, knobW, 20);
}

void MobiusSpectralCancellerAudioProcessorEditor::timerCallback()
{
    updateMaskVisual();
    repaint();
}

void MobiusSpectralCancellerAudioProcessorEditor::updateMaskVisual()
{
    const float scale = processor.getAPVTS().getRawParameterValue ("scale")->load();
    const float depth = processor.getAPVTS().getRawParameterValue ("depth")->load();
    const auto& table = processor.getMobiusTable();
    const int fftSize = processor.getFftSize();
    const int numBins = fftSize / 2 + 1;

    juce::Graphics g (maskImage);
    g.fillAll (juce::Colours::black);

    for (int x = 0; x < maskImage.getWidth(); ++x)
    {
        const int k = juce::jmap (x, 0, maskImage.getWidth() - 1, 1, numBins - 2);
        const int n = juce::roundToInt (static_cast<float> (k) * scale);
        if (n < 1 || n > table.getMaxN())
            continue;

        const int8_t mu = table[n];
        juce::Colour c;
        if (mu == 0)
            c = juce::Colours::black;
        else if (mu > 0)
            c = juce::Colours::limegreen.withBrightness (0.3f + 0.7f * std::abs (depth));
        else
            c = juce::Colours::red.withBrightness (0.3f + 0.7f * std::abs (depth));

        g.setColour (c);
        g.drawVerticalLine (x, 0.0f, static_cast<float> (maskImage.getHeight()));
    }
}
