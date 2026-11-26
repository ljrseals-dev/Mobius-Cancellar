#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class MobiusSpectralCancellerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit MobiusSpectralCancellerAudioProcessorEditor (MobiusSpectralCancellerAudioProcessor&);
    ~MobiusSpectralCancellerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MobiusSpectralCancellerAudioProcessor& processor;

    juce::Slider depthSlider, scaleSlider, mixSlider;
    juce::Label  depthLabel,  scaleLabel,  mixLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> scaleAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    // Simple visual of the current Möbius mask (one pixel per bin)
    juce::Image maskImage;
    void updateMaskVisual();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobiusSpectralCancellerAudioProcessorEditor)
};
