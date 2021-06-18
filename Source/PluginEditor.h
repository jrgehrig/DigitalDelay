/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class DigitalDelayAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Button::Listener 
{
public:
    DigitalDelayAudioProcessorEditor (DigitalDelayAudioProcessor&);
    ~DigitalDelayAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    void createSliderAttachments();
    void createButtonAttachments(); 
    void buttonClicked(juce::Button* ) override;
    void setTimeValFromText();

private:
    juce::Slider            feedbackSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider                 panSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider              dryWetSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::ArrowButton       increaseButton;
    juce::ArrowButton       decreaseButton;
    juce::ToggleButton  millisecondsButton;
    juce::ToggleButton         stepsButton;
    juce::ToggleButton sixteenthNoteButton;
    juce::ToggleButton eighthTripletButton;
    juce::TextEditor               display;

    juce::Label              feedbackLabel;
    juce::Label                   panLabel;
    juce::Label                dryWetLabel;
    juce::Label          millisecondsLabel;
    juce::Label                 stepsLabel;
    juce::Label         sixteenthNoteLabel;
    juce::Label         eighthTripletLabel;

    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;

    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow; 
    
    DigitalDelayAudioProcessor& audioProcessor;

    int testValSteps;
    int testValMs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalDelayAudioProcessorEditor)
};
