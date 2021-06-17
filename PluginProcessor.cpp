/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
DigitalDelayAudioProcessor::DigitalDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), tree(*this, nullptr, "PARAMS", createParameterLayout()), lastSampleRate(44100.0f),
                          feedback(std::sqrt(0.5f)), dryWet(std::sqrt(0.5f)), pan(0.0f), steps(1), lastDryWet(std::sqrt(0.5f)),
                          dryGain(dryWet), lastDryGain(lastDryWet)

#endif
{
    tree.addParameterListener(getFeedbackParamName(), this);
    tree.addParameterListener(getPanParamName(), this);
    tree.addParameterListener(getDryWetParamName(), this);
    tree.addParameterListener(getMsecParamName(), this); //may not need this
    tree.addParameterListener(getStepsParamName(), this);
    delayBuffer.clear();
    dryBuffer.clear();
    convertStepsToMsec();
}

DigitalDelayAudioProcessor::~DigitalDelayAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DigitalDelayAudioProcessor::createParameterLayout()
{
    juce::NormalisableRange<float> feedbackRange (0.0f, 1.0f);
    juce::NormalisableRange<float> dryWetRange   (0.0f, 1.0f);
    juce::NormalisableRange<float> panRange      (-1.0f, 1.0f);

    std::vector <std::unique_ptr<juce::RangedAudioParameter>> params;

    auto feedbackParam = std::make_unique<juce::AudioParameterFloat> (getFeedbackParamName(), 
                         getFeedbackParamName(), feedbackRange, 0.5f,
                         juce::String(), juce::AudioProcessorParameter::genericParameter,
                         [](float param, int) {return juce::String(param * 100, 1) + "%"; });
    params.push_back(std::move(feedbackParam));

    auto dryWetParam   = std::make_unique<juce::AudioParameterFloat>(getDryWetParamName(),
                         getDryWetParamName(), dryWetRange, 0.5f,
                         juce::String(), juce::AudioProcessorParameter::genericParameter,
                         [](float param, int) {return juce::String(param * 100, 1) + "%"; });
    params.push_back(std::move(dryWetParam));

    auto panParam      = std::make_unique<juce::AudioParameterFloat>(getPanParamName(),
                         getPanParamName(), panRange, 0.0f,
                         juce::String(), juce::AudioProcessorParameter::genericParameter,
                         [](float param, int) {return param >= 0 ? juce::String(param * 100, 1) + "% R" 
                         : juce::String(-100 * param,1) + "% L"; });
    params.push_back(std::move(panParam));

    auto msecParam     = std::make_unique<juce::AudioParameterInt>(getMsecParamName(),
                         getMsecParamName(), 1, 2000, 1, juce::String(),
                         [](int param, int) {return juce::String(param); });
    params.push_back(std::move(msecParam));
    
    auto stepsParam    = std::make_unique<juce::AudioParameterInt>(getStepsParamName(),
                         getStepsParamName(), 1, 16, 1, juce::String(),
                         [](int param, int) {return juce::String(param); });
    params.push_back(std::move(stepsParam));

    return { params.begin(), params.end() };

}

void DigitalDelayAudioProcessor::parameterChanged(const juce::String& parameter, float newValue)
{
    if (parameter == getFeedbackParamName())
    {
        feedback = std::sqrt(newValue);
    }
    else if (parameter == getDryWetParamName())
    {
        dryWet = std::sqrt(newValue);
        dryGain = std::sqrt(1 - dryWet);
    }
    else if (parameter == getPanParamName())
    {
        panGains[0] = newValue <= 0 ? 1.0f : std::sqrt(1.0f - newValue); //left gain
        panGains[1] = newValue >= 0 ? 1.0f : std::sqrt(1.0f + newValue);
    }
    else if (parameter == getMsecParamName())
    {
        msec = newValue;
        DBG("msec changed");
    }
    else if (parameter == getStepsParamName())
    {
        steps = newValue;
        convertStepsToMsec();
        DBG("steps changed");
    }
}

void DigitalDelayAudioProcessor::convertStepsToMsec()
{
    if (isEighthTripletActive())
        msec = juce::roundToInt(60000 * steps / (3 * tempo));
    else
        msec = juce::roundToInt(60000 * steps / (4 * tempo));
}

//==============================================================================
const juce::String DigitalDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DigitalDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DigitalDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DigitalDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DigitalDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DigitalDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DigitalDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DigitalDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DigitalDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void DigitalDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DigitalDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    const int numInputChannels = getTotalNumInputChannels();
    const int delayBufferSize = 2 * (sampleRate + samplesPerBlock); //2 seconds max delay and 2 buffers
    delayBuffer.setSize(numInputChannels, delayBufferSize);
    dryBuffer.setSize(numInputChannels, samplesPerBlock);
}

void DigitalDelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DigitalDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DigitalDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    playHead = this->getPlayHead();
    playHead->getCurrentPosition(sessionInfo);
    tempo = sessionInfo.bpm;

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    float delayGainL = dryWet * panGains[0];
    float delayGainR = dryWet * panGains[1];
    float lastDelayGainL = lastDryWet * lastPanGains[0];
    float lastDelayGainR = lastDryWet * lastPanGains[1];
    float delayGains[2] = { delayGainL,delayGainR };
    float lastDelayGains[2] = { lastDelayGainL,lastDelayGainR };

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
        delayBuffer.clear(i, 0, delayBuffer.getNumSamples());
        dryBuffer.clear(i, 0, dryBuffer.getNumSamples());
    }

    int delaySamples = static_cast<int> (lastSampleRate * msec / 1000);
    const int readPosition = (delayBuffer.getNumSamples() + writePosition - delaySamples) % delayBuffer.getNumSamples();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples()); //fill dry buffer
        fillDelayBuffer(buffer, channel, writePosition, true);
    }

    if (expectedReadPos >= 0)
    {  
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float endGain = (readPosition == expectedReadPos) ? delayGains[channel] : 0.0f;
            float startGain = (delayGains[channel] == lastDelayGains[channel]) ? delayGains[channel] : lastDelayGains[channel];
            getFromDelayBuffer(buffer, channel, expectedReadPos, startGain, endGain);
        }
    }

    if (readPosition != expectedReadPos)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float endGain = (delayGains[channel] == lastDelayGains[channel]) ? delayGains[channel] : lastDelayGains[channel];
            getFromDelayBuffer(buffer, channel, readPosition, 0.0f, endGain);
        }
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        applyFeedback(buffer, channel, writePosition); 
        buffer.addFromWithRamp(channel, 0, dryBuffer.getReadPointer(channel), buffer.getNumSamples(), lastDryGain, dryGain);
        lastPanGains[channel] = panGains[channel];
    }

    lastFeedback = feedback; 
    lastDryWet = dryWet; 
    lastDryGain = dryGain;

    writePosition += buffer.getNumSamples();
    writePosition %= delayBuffer.getNumSamples();

    expectedReadPos = readPosition + buffer.getNumSamples();
    expectedReadPos %= delayBuffer.getNumSamples(); 
}

void DigitalDelayAudioProcessor::fillDelayBuffer(juce::AudioBuffer<float>& buffer,
    const int channel, const int writePosition, bool replacing)
{
    if (delayBuffer.getNumSamples() > buffer.getNumSamples() + writePosition)
    {
        if (replacing)
            delayBuffer.copyFrom(channel, writePosition, buffer.getReadPointer(channel), buffer.getNumSamples());//, lastPanGains[channel], panGains[channel]);
        else
            delayBuffer.addFromWithRamp (channel, writePosition, buffer.getReadPointer(channel), buffer.getNumSamples(), lastPanGains[channel], panGains[channel]);
    }
    else
    {
        const int bufferRemaining = delayBuffer.getNumSamples() - writePosition;
        if (replacing)
        {
            delayBuffer.copyFrom(channel, writePosition, buffer.getReadPointer(channel), bufferRemaining);// , lastPanGains[channel], panGains[channel]);
            delayBuffer.copyFrom(channel, 0, buffer.getReadPointer(channel), buffer.getNumSamples() - bufferRemaining);// , lastPanGains[channel], panGains[channel]);
        }
        else
        {
            delayBuffer.addFromWithRamp (channel, writePosition, buffer.getReadPointer(channel), bufferRemaining, lastPanGains[channel], panGains[channel]);
            delayBuffer.addFromWithRamp (channel, 0, buffer.getReadPointer(channel), buffer.getNumSamples() - bufferRemaining, lastPanGains[channel], panGains[channel]);
        } 
    }
}

void DigitalDelayAudioProcessor::getFromDelayBuffer(juce::AudioBuffer<float>& buffer, int channel, int readPosition, float startGain, float endGain)
{
    if (delayBuffer.getNumSamples() > buffer.getNumSamples() + readPosition)
    {
        buffer.copyFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), buffer.getNumSamples(), startGain, endGain);
    }
    else
    {
        const int bufferRemaining = delayBuffer.getNumSamples() - readPosition;
        const float gainSwitch = juce::jmap(float(bufferRemaining) / buffer.getNumSamples(), startGain, endGain);
        buffer.copyFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), bufferRemaining, startGain, gainSwitch);
        buffer.copyFromWithRamp(channel, bufferRemaining, delayBuffer.getReadPointer(channel), buffer.getNumSamples() - bufferRemaining, gainSwitch, endGain);
    }
}

void DigitalDelayAudioProcessor::applyFeedback(juce::AudioBuffer<float>& buffer,
    const int channel, const int writePosition)
{
    if (delayBuffer.getNumSamples() > buffer.getNumSamples() + writePosition)
    {
        delayBuffer.addFromWithRamp(channel, writePosition, buffer.getReadPointer(channel), buffer.getNumSamples(), lastFeedback, feedback);
    }
    else
    {
        const int bufferRemaining = delayBuffer.getNumSamples() - writePosition;
        delayBuffer.addFromWithRamp(channel, bufferRemaining, buffer.getReadPointer(channel), bufferRemaining, lastFeedback, feedback);
        delayBuffer.addFromWithRamp(channel, 0, buffer.getReadPointer(channel), buffer.getNumSamples() - bufferRemaining, lastFeedback, feedback);
    }
}

//==============================================================================
bool DigitalDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DigitalDelayAudioProcessor::createEditor()
{
    return new DigitalDelayAudioProcessorEditor (*this);
}

//==============================================================================
void DigitalDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void DigitalDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DigitalDelayAudioProcessor();
}

juce::String DigitalDelayAudioProcessor::getFeedbackParamName()
{
    return juce::String("Feedback");
}
juce::String DigitalDelayAudioProcessor::getPanParamName()
{
    return juce::String("Pan");
}
juce::String DigitalDelayAudioProcessor::getDryWetParamName()
{
    return juce::String("DryWet");
}
juce::String DigitalDelayAudioProcessor::getMsecParamName()
{
    return juce::String("Milliseconds");
}
juce::String DigitalDelayAudioProcessor::getStepsParamName()
{
    return juce::String("Steps");
}
juce::String DigitalDelayAudioProcessor::getIncreaseParamName()
{
    return juce::String("Increase");
}
juce::String DigitalDelayAudioProcessor::getDecreaseParamName()
{
    return juce::String("Decrease");
}

bool DigitalDelayAudioProcessor::isMillisecondsActive()
{
    return millisecondsActive;
}
bool DigitalDelayAudioProcessor::isStepsActive()
{
    return stepsActive;
}
bool DigitalDelayAudioProcessor::isSixteenthNoteActive()
{
    return sixteenthNoteActive;
}
bool DigitalDelayAudioProcessor::isEighthTripletActive()
{
    return eighthTripletActive;
}

void DigitalDelayAudioProcessor::setMillisecondsActive(bool newState)
{
    millisecondsActive = newState;
}
void DigitalDelayAudioProcessor::setStepsActive(bool newState)
{
    stepsActive = newState;
}
void DigitalDelayAudioProcessor::setSixteenthNoteActive(bool newState)
{
    sixteenthNoteActive = newState;
}
void DigitalDelayAudioProcessor::setEighthTripletActive(bool newState)
{
    eighthTripletActive = newState;
}