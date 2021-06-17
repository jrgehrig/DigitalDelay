/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class DigitalDelayAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    DigitalDelayAudioProcessor();
    ~DigitalDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::String getFeedbackParamName();
    juce::String getPanParamName();
    juce::String getDryWetParamName(); 
    juce::String getMsecParamName();
    juce::String getStepsParamName();
    juce::String getIncreaseParamName();
    juce::String getDecreaseParamName();

    bool isMillisecondsActive();
    bool isStepsActive();
    bool isSixteenthNoteActive();
    bool isEighthTripletActive();

    void setMillisecondsActive(bool);
    void setStepsActive(bool);
    void setSixteenthNoteActive(bool);
    void setEighthTripletActive(bool);

    void fillDelayBuffer(juce::AudioBuffer<float>& buffer,
        const int channelIn, const int writePos, bool replacing);
    void getFromDelayBuffer(juce::AudioBuffer<float>& buffer, int channel, int readPosition, float startGain, float endGain);
    void applyFeedback(juce::AudioBuffer<float>& buffer,
        const int channel, const int writePosition);

    //bool getCurrentPosition(CurrentPositionInfo& result) override;

    juce::AudioProcessorValueTreeState tree;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameter, float newValue) override;

    void convertStepsToMsec();

    int   msec;
    int   steps;
private:
    juce::AudioBuffer<float> delayBuffer;
    int expectedReadPos{ -1 };
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioPlayHead* playHead;
    juce::AudioPlayHead::CurrentPositionInfo sessionInfo;

    int writePosition { 0 };
    bool millisecondsActive  { false };
    bool stepsActive         { true };
    bool sixteenthNoteActive { true };
    bool eighthTripletActive { false };

    float lastSampleRate;
    float feedback;
    float lastFeedback{ 0.5f };
    float dryWet;
    float lastDryWet;
    float dryGain;
    float lastDryGain;
    float pan; //can probably remove
    float panGains[2]{ 1.0f,1.0f };
    float lastPanGains[2]{ 1.0f,1.0f };

    float tempo{ 120 };
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalDelayAudioProcessor)
};
