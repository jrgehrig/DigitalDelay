/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DigitalDelayAudioProcessorEditor::DigitalDelayAudioProcessorEditor(DigitalDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), increaseButton(juce::String("increase"), 0.75, juce::Colours::aqua),
    decreaseButton(juce::String("decrease"), 0.25, juce::Colours::aqua), testValSteps(1), testValMs(1)
{
    tooltipWindow->setMillisecondsBeforeTipAppears(1000);

    addAndMakeVisible(feedbackSlider);
    feedbackSlider.setTooltip(juce::String("Change the feedback of the delay."));
    addAndMakeVisible(panSlider);
    panSlider.setTooltip(juce::String("Change the panning of the wet signal."));
    addAndMakeVisible(dryWetSlider);
    dryWetSlider.setTooltip(juce::String("Change the dry/wet blend."));
    createSliderAttachments();

    addAndMakeVisible(feedbackLabel);
    feedbackLabel.setText(juce::String("Feedback"), juce::NotificationType::dontSendNotification);
    feedbackLabel.setJustificationType(juce::Justification::horizontallyCentred);
    feedbackLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(panLabel);
    panLabel.setText(juce::String("Pan"), juce::NotificationType::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::horizontallyCentred);
    panLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(dryWetLabel);
    dryWetLabel.setText(juce::String("Dry/Wet"), juce::NotificationType::dontSendNotification);
    dryWetLabel.setJustificationType(juce::Justification::horizontallyCentred);
    dryWetLabel.setFont(juce::Font(12.0f));
    
    addAndMakeVisible(millisecondsButton);
    //Only want to be able to toggle a button on if it is currently off, implement further in buttonClicked()
    millisecondsButton.setClickingTogglesState(!audioProcessor.isMillisecondsActive());
    //Setting the toggle states according to flags in the processor to ensure they are correct upon construction
    millisecondsButton.setToggleState(audioProcessor.isMillisecondsActive(), juce::NotificationType::dontSendNotification);
    millisecondsButton.addListener(this);
    millisecondsButton.setTooltip(juce::String("Set delay time in milliseconds."));

    addAndMakeVisible(stepsButton);
    stepsButton.setClickingTogglesState(!audioProcessor.isStepsActive());
    stepsButton.setToggleState(audioProcessor.isStepsActive(), juce::NotificationType::dontSendNotification);
    stepsButton.addListener(this);
    stepsButton.setTooltip(juce::String("Set delay time in tempo synced steps."));

    addAndMakeVisible(millisecondsLabel);
    millisecondsLabel.setText(juce::String("MS"), juce::NotificationType::dontSendNotification);
    millisecondsLabel.setJustificationType(juce::Justification::centredLeft);
    millisecondsLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(stepsLabel);
    stepsLabel.setText(juce::String("Steps"), juce::NotificationType::dontSendNotification);
    stepsLabel.setJustificationType(juce::Justification::centredLeft);
    stepsLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(sixteenthNoteButton);
    sixteenthNoteButton.setClickingTogglesState(!audioProcessor.isSixteenthNoteActive());
    sixteenthNoteButton.setToggleState(audioProcessor.isSixteenthNoteActive(), juce::NotificationType::dontSendNotification);
    sixteenthNoteButton.addListener(this);
    sixteenthNoteButton.setTooltip(juce::String("Set delay time in sixteenth note steps. Disabled if time is being set in milliseconds."));

    addAndMakeVisible(eighthTripletButton);
    eighthTripletButton.setClickingTogglesState(!audioProcessor.isEighthTripletActive());
    eighthTripletButton.setToggleState(audioProcessor.isEighthTripletActive(), juce::NotificationType::dontSendNotification);
    eighthTripletButton.addListener(this);
    eighthTripletButton.setTooltip(juce::String("Set delay time in eighth note tripled steps. Disabled if time is being set in milliseconds."));

    addAndMakeVisible(sixteenthNoteLabel);
    sixteenthNoteLabel.setText(juce::String("1/16"), juce::NotificationType::dontSendNotification);
    sixteenthNoteLabel.setJustificationType(juce::Justification::centredLeft);
    sixteenthNoteLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(eighthTripletLabel);
    eighthTripletLabel.setText(juce::String("1/8T"), juce::NotificationType::dontSendNotification);
    eighthTripletLabel.setJustificationType(juce::Justification::centredLeft);
    eighthTripletLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(increaseButton);
    increaseButton.addListener(this); 
    increaseButton.setRepeatSpeed(500, 15, -1);
    increaseButton.setTooltip(juce::String("Increase the delay time. Delay time can also be typed on the screen."));

    addAndMakeVisible(decreaseButton);
    decreaseButton.addListener(this);
    decreaseButton.setRepeatSpeed(500, 15, -1);
    decreaseButton.setTooltip(juce::String("Decrease the delay time. Delay time can also be typed on the screen."));

    addAndMakeVisible(display);
    display.setMultiLine(false);
    display.setJustification(juce::Justification::centredRight);
    juce::Font editorFont; 
    editorFont.setTypefaceName("Courier new");
    editorFont.setSizeAndStyle(56, "Arial", 1, 0);
    display.setFont(editorFont);
    if (audioProcessor.isStepsActive())
        display.setText(juce::String(audioProcessor.steps));
    else
        display.setText(juce::String(audioProcessor.msec));
    display.onReturnKey = [this]() { setTimeValFromText(); };

    setSize (650, 150);
}

DigitalDelayAudioProcessorEditor::~DigitalDelayAudioProcessorEditor()
{
}

//==============================================================================
void DigitalDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);

}

void DigitalDelayAudioProcessorEditor::resized()
{
    int sliderSide = 85;
    int buttonSide = 32; 
    int buttonStartX = 230;
    int buttonStartY = 50;
    feedbackSlider.setBounds(380, 45, sliderSide, sliderSide);
    feedbackLabel.setBounds(380, 135, sliderSide, 12);

    panSlider.setBounds(470, 45, sliderSide, sliderSide);
    panLabel.setBounds(470, 135, sliderSide, 12);

    dryWetSlider.setBounds(560, 45, sliderSide, sliderSide);
    dryWetLabel.setBounds(560, 135, sliderSide, 12);

    millisecondsButton.setBounds(buttonStartX, buttonStartY, buttonSide, buttonSide);
    millisecondsLabel.setBounds(millisecondsButton.getX() + millisecondsButton.getWidth() / 2 + 5, millisecondsButton.getY() + millisecondsButton.getWidth() / 3, 30, 12);

    stepsButton.setBounds(buttonStartX, buttonStartY + buttonSide, buttonSide, buttonSide);
    stepsLabel.setBounds(stepsButton.getX() + stepsButton.getWidth() / 2 + 5, stepsButton.getY() + stepsButton.getWidth() / 3, 60, 12);

    sixteenthNoteButton.setBounds(buttonStartX + 80, buttonStartY, buttonSide, buttonSide);
    sixteenthNoteLabel.setBounds(sixteenthNoteButton.getX() + sixteenthNoteButton.getWidth() / 2 + 5, sixteenthNoteButton.getY() + sixteenthNoteButton.getWidth() / 3, 60, 12);

    eighthTripletButton.setBounds(buttonStartX + 80, buttonStartY + buttonSide, buttonSide, buttonSide);
    eighthTripletLabel.setBounds(eighthTripletButton.getX() + eighthTripletButton.getWidth() / 2 + 5, eighthTripletButton.getY() + eighthTripletButton.getWidth() / 3, 60, 12);
    
    display.setBounds(10, millisecondsButton.getY(), 150, 60);
    increaseButton.setBounds(display.getRight() + 10, display.getY(), 24, 24);
    decreaseButton.setBounds(display.getRight() + 10, display.getBottom() - 24, 24, 24);
}

void DigitalDelayAudioProcessorEditor::createSliderAttachments()
{
    sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment
    (audioProcessor.tree, audioProcessor.getFeedbackParamName(), feedbackSlider));
    sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment
    (audioProcessor.tree, audioProcessor.getPanParamName(), panSlider));
    sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment
    (audioProcessor.tree, audioProcessor.getDryWetParamName(), dryWetSlider));
}
void DigitalDelayAudioProcessorEditor::createButtonAttachments()
{
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment
    (audioProcessor.tree, audioProcessor.getStepsParamName(), increaseButton));
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment
    (audioProcessor.tree, audioProcessor.getStepsParamName(), decreaseButton));
}

void DigitalDelayAudioProcessorEditor::setTimeValFromText()
{
    int newVal = display.getText().getIntValue();
    if (audioProcessor.isStepsActive())
    { 
        if (newVal < 1)
            audioProcessor.steps = 1;
        else if (newVal > 16)
            audioProcessor.steps = 16;
        else
            audioProcessor.steps = newVal;
        display.setText(juce::String(audioProcessor.steps));
    }
    else
    { 
        if (newVal < 1)
            audioProcessor.msec = 1;
        else if (newVal > 2000)
            audioProcessor.msec = 2000;
        else
            audioProcessor.msec = newVal;
        display.setText(juce::String(audioProcessor.msec));
    }
}

void DigitalDelayAudioProcessorEditor::buttonClicked(juce::Button* b)
{
    if (b == &millisecondsButton && !audioProcessor.isMillisecondsActive())
    {
        audioProcessor.setMillisecondsActive(true);
        millisecondsButton.setClickingTogglesState(false);
        audioProcessor.convertStepsToMsec();
        display.setText(juce::String(audioProcessor.msec));

        audioProcessor.setStepsActive(false);
        stepsButton.setClickingTogglesState(true);
        stepsButton.setToggleState(false, juce::NotificationType::dontSendNotification);

        eighthTripletButton.setEnabled(false);
        sixteenthNoteButton.setEnabled(false);
    }
    else if (b == &stepsButton && !audioProcessor.isStepsActive())
    {
        audioProcessor.setStepsActive(true);
        stepsButton.setClickingTogglesState(false);
        audioProcessor.convertStepsToMsec();
        display.setText(juce::String(audioProcessor.steps));
        
        audioProcessor.setMillisecondsActive(false);
        millisecondsButton.setClickingTogglesState(true);
        millisecondsButton.setToggleState(false, juce::NotificationType::dontSendNotification);

        eighthTripletButton.setEnabled(true);
        sixteenthNoteButton.setEnabled(true);
    }
    else if (b == &sixteenthNoteButton && !audioProcessor.isSixteenthNoteActive())
    {
        audioProcessor.setSixteenthNoteActive(true);
        audioProcessor.convertStepsToMsec();
        sixteenthNoteButton.setClickingTogglesState(false);

        audioProcessor.setEighthTripletActive(false);
        eighthTripletButton.setClickingTogglesState(true);
        eighthTripletButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (b == &eighthTripletButton && !audioProcessor.isEighthTripletActive())
    {
        audioProcessor.setEighthTripletActive(true);
        audioProcessor.convertStepsToMsec();
        eighthTripletButton.setClickingTogglesState(false);

        audioProcessor.setSixteenthNoteActive(false);
        sixteenthNoteButton.setClickingTogglesState(true);
        sixteenthNoteButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (b == &increaseButton)
    {
        if (audioProcessor.isStepsActive() && audioProcessor.steps >= 1 && audioProcessor.steps < 16)
        {
            ++audioProcessor.steps;
            audioProcessor.convertStepsToMsec();
            display.setText(juce::String(audioProcessor.steps));
        }
        else if (audioProcessor.isMillisecondsActive() && audioProcessor.msec >= 1 && audioProcessor.msec < 2000)
        {
            ++audioProcessor.msec;
            display.setText(juce::String(audioProcessor.msec));
        }
        
    }
    else if (b == &decreaseButton)
    {
        if (audioProcessor.isStepsActive() && audioProcessor.steps > 1 && audioProcessor.steps <= 16)
        {
            --audioProcessor.steps;
            audioProcessor.convertStepsToMsec();
            display.setText(juce::String(audioProcessor.steps));
        }
        else if (audioProcessor.isMillisecondsActive() && audioProcessor.msec > 1 && audioProcessor.msec <= 2000)
        {
            --audioProcessor.msec;
            display.setText(juce::String(audioProcessor.msec));
        }
    }
}
