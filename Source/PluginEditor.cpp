#include "PluginEditor.h"

FeedbackControlAudioProcessorEditor::FeedbackControlAudioProcessorEditor (FeedbackControlAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (820, 510);
    title.setText ("FEEDBACK CONTROL", juce::dontSendNotification);
    title.setFont (juce::Font (25.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (title);

    subtitle.setText ("AUTOMATIC FEEDBACK CONTROL  /  PROTOTYPE v0.2", juce::dontSendNotification);
    subtitle.setFont (juce::Font (12.0f));
    subtitle.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (subtitle);

    detectorStatus.setText ("Detector ready — waiting for stable narrow tone", juce::dontSendNotification);
    detectorStatus.setColour (juce::Label::textColourId, juce::Colour (0xff50d8bd));
    detectorStatus.setFont (juce::Font (13.0f));
    addAndMakeVisible (detectorStatus);

    addAndMakeVisible (automaticButton);
    autoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "autoEnable", automaticButton);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bypass", bypassButton);

    sensitivityLabel.setText ("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (sensitivityLabel);
    sensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sensitivitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 22);
    addAndMakeVisible (sensitivitySlider);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "sensitivity", sensitivitySlider);

    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String (i + 1);
        notchLabels[static_cast<size_t>(i)].setText ("MANUAL NOTCH " + n, juce::dontSendNotification);
        notchLabels[static_cast<size_t>(i)].setColour (juce::Label::textColourId, juce::Colours::white);
        notchLabels[static_cast<size_t>(i)].setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (notchLabels[static_cast<size_t>(i)]);

        enableButtons[static_cast<size_t>(i)].setButtonText ("Enable");
        addAndMakeVisible (enableButtons[static_cast<size_t>(i)]);
        enableAttachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                audioProcessor.parameters, "enable" + n, enableButtons[static_cast<size_t>(i)]);

        auto setupSlider = [this] (juce::Slider& s)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
            addAndMakeVisible (s);
        };
        setupSlider (frequencySliders[static_cast<size_t>(i)]);
        setupSlider (depthSliders[static_cast<size_t>(i)]);
        setupSlider (qSliders[static_cast<size_t>(i)]);
        frequencySliders[static_cast<size_t>(i)].setTextValueSuffix (" Hz");
        depthSliders[static_cast<size_t>(i)].setTextValueSuffix (" dB");
        qSliders[static_cast<size_t>(i)].setTextValueSuffix (" Q");

        freqAttachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters, "freq" + n, frequencySliders[static_cast<size_t>(i)]);
        depthAttachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters, "depth" + n, depthSliders[static_cast<size_t>(i)]);
        qAttachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters, "q" + n, qSliders[static_cast<size_t>(i)]);
    }
    startTimerHz (8);
}

void FeedbackControlAudioProcessorEditor::timerCallback()
{
    const float frequency = audioProcessor.getDetectedFrequency();
    const int active = audioProcessor.getActiveAutoNotches();
    if (frequency > 0.0f)
        detectorStatus.setText ("Last detected candidate: " + juce::String (frequency, 1)
                                + " Hz   |   Auto notches active: " + juce::String (active),
                                juce::dontSendNotification);
    else
        detectorStatus.setText ("Detector ready — auto notches active: " + juce::String (active),
                                juce::dontSendNotification);
}

void FeedbackControlAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff10151c));
    g.setColour (juce::Colour (0xff283543));
    g.drawRoundedRectangle (12.0f, 12.0f, static_cast<float>(getWidth() - 24),
                            static_cast<float>(getHeight() - 24), 10.0f, 1.0f);
    g.setColour (juce::Colour (0xff50d8bd));
    g.fillRoundedRectangle (12.0f, 12.0f, 5.0f, static_cast<float>(getHeight() - 24), 2.0f);
    g.setColour (juce::Colours::lightgrey);
    g.setFont (12.0f);
    g.drawText ("Frequency              Depth                 Q", 28, 236, getWidth() - 56, 20,
                juce::Justification::centred);
    g.drawText ("Manual filters are for operator intervention; automatic filters are inserted by the detector.",
                24, getHeight() - 28, getWidth() - 48, 16, juce::Justification::centred);
}

void FeedbackControlAudioProcessorEditor::resized()
{
    title.setBounds (30, 22, 430, 36);
    subtitle.setBounds (32, 59, 460, 22);
    automaticButton.setBounds (32, 91, 130, 28);
    bypassButton.setBounds (getWidth() - 122, 30, 88, 30);
    sensitivityLabel.setBounds (185, 92, 80, 24);
    sensitivitySlider.setBounds (265, 91, 210, 26);
    detectorStatus.setBounds (32, 126, getWidth() - 64, 24);

    const int left = 24;
    const int colWidth = (getWidth() - 48) / 4;
    for (int i = 0; i < 4; ++i)
    {
        const int x = left + i * colWidth;
        notchLabels[static_cast<size_t>(i)].setBounds (x + 5, 162, colWidth - 10, 22);
        enableButtons[static_cast<size_t>(i)].setBounds (x + 5, 186, colWidth - 10, 24);
        frequencySliders[static_cast<size_t>(i)].setBounds (x + 3, 252, colWidth - 6, 105);
        depthSliders[static_cast<size_t>(i)].setBounds (x + 3, 348, colWidth - 6, 85);
        qSliders[static_cast<size_t>(i)].setBounds (x + 3, 414, colWidth - 6, 76);
    }
}
