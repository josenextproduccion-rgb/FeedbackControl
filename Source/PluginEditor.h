#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class FeedbackControlAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit FeedbackControlAudioProcessorEditor (FeedbackControlAudioProcessor&);
    ~FeedbackControlAudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    FeedbackControlAudioProcessor& audioProcessor;
    juce::Label title;
    juce::Label subtitle;
    juce::Label detectorStatus;
    juce::ToggleButton automaticButton { "AUTO DETECT" };
    juce::TextButton bypassButton { "BYPASS" };
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    std::array<juce::Label, 4> notchLabels;
    std::array<juce::ToggleButton, 4> enableButtons;
    std::array<juce::Slider, 4> frequencySliders;
    std::array<juce::Slider, 4> depthSliders;
    std::array<juce::Slider, 4> qSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> freqAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> depthAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> qAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 4> enableAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackControlAudioProcessorEditor)
};
