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
    tree.state.setProperty(getStepsParamName(), steps, nullptr);
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
    if (isStepsActive())
    {
        if (isEighthTripletActive())
            msec = juce::roundToInt(60000 * steps / (3 * tempo));
        else
            msec = juce::roundToInt(60000 * steps / (4 * tempo));
    }
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
    expectedReadPos = -1;
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

void DigitalDelayAudioProcessor::processBlock(juce::AudioSampleBuffer& buffer, juce::MidiBuffer& midiMessages)
{
    playHead = this->getPlayHead();
    playHead->getCurrentPosition(sessionInfo);
    tempo = sessionInfo.bpm;
    convertStepsToMsec();

    if (Bus* inputBus = getBus(true, 0))
    {
        const float gain = dryGain;
        const float wetGain[2] = { dryWet * panGains[0],dryWet * panGains[1] };
        const float time = msec;
        const float feedback = this->feedback;

        // write original to delay
        for (int i = 0; i < delayBuffer.getNumChannels(); ++i)
        {
            const int inputChannelNum = inputBus->getChannelIndexInProcessBlockBuffer(std::min(i, inputBus->getNumberOfChannels()));
            writeToDelayBuffer(buffer, inputChannelNum, i, writePosition, 1.0f, 1.0f, true);
        }

        // adapt dry gain
        buffer.applyGainRamp(0, buffer.getNumSamples(), lastDryGain, gain);
        lastDryGain = gain;

        // read delayed signal
        auto readPos = juce::roundToInt(writePosition - (lastSampleRate * time / 1000.0));
        if (readPos < 0)
            readPos += delayBuffer.getNumSamples();

        if (Bus* outputBus = getBus(false, 0))
        {
            // if has run before
            if (expectedReadPos >= 0)
            {
                // fade out if readPos is off
                
                for (int i = 0; i < outputBus->getNumberOfChannels(); ++i)
                {
                    auto endGain = (readPos == expectedReadPos) ? wetGain[i] : 0.0f;
                    const int outputChannelNum = outputBus->getChannelIndexInProcessBlockBuffer(i);
                    readFromDelayBuffer(buffer, i, outputChannelNum, expectedReadPos, wetGain[i], endGain, false);
                }
            }

            // fade in at new position
            if (readPos != expectedReadPos)
            {
                for (int i = 0; i < outputBus->getNumberOfChannels(); ++i)
                {
                    const int outputChannelNum = outputBus->getChannelIndexInProcessBlockBuffer(i);
                    readFromDelayBuffer(buffer, i, outputChannelNum, readPos, 0.0, wetGain[i], false);
                }
            }
        }

        // add feedback to delay
        for (int i = 0; i < inputBus->getNumberOfChannels(); ++i)
        {
            const int outputChannelNum = inputBus->getChannelIndexInProcessBlockBuffer(i);
            writeToDelayBuffer(buffer, outputChannelNum, i, writePosition, lastFeedback, feedback, false);
        }
        lastFeedback = feedback;

        // advance positions
        writePosition += buffer.getNumSamples();
        if (writePosition >= delayBuffer.getNumSamples())
            writePosition -= delayBuffer.getNumSamples();

        expectedReadPos = readPos + buffer.getNumSamples();
        if (expectedReadPos >= delayBuffer.getNumSamples())
            expectedReadPos -= delayBuffer.getNumSamples();
    }
}

void DigitalDelayAudioProcessor::writeToDelayBuffer(juce::AudioSampleBuffer& buffer,
    const int channelIn, const int channelOut,
    const int writePos, float startGain, float endGain, bool replacing)
{
    if (writePos + buffer.getNumSamples() <= delayBuffer.getNumSamples())
    {
        if (replacing)
            delayBuffer.copyFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), buffer.getNumSamples(), startGain, endGain);
        else
            delayBuffer.addFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), buffer.getNumSamples(), startGain, endGain);
    }
    else
    {
        const auto midPos = delayBuffer.getNumSamples() - writePos;
        const auto midGain = juce::jmap(float(midPos) / buffer.getNumSamples(), startGain, endGain);
        if (replacing)
        {
            delayBuffer.copyFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), midPos, startGain, midGain);
            delayBuffer.copyFromWithRamp(channelOut, 0, buffer.getReadPointer(channelIn, midPos), buffer.getNumSamples() - midPos, midGain, endGain);
        }
        else
        {
            delayBuffer.addFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), midPos, lastDryGain, midGain);
            delayBuffer.addFromWithRamp(channelOut, 0, buffer.getReadPointer(channelIn, midPos), buffer.getNumSamples() - midPos, midGain, endGain);
        }
    }
}

void DigitalDelayAudioProcessor::readFromDelayBuffer(juce::AudioSampleBuffer& buffer,
    const int channelIn, const int channelOut,
    const int readPos,
    float startGain, float endGain,
    bool replacing)
{
    if (readPos + buffer.getNumSamples() <= delayBuffer.getNumSamples())
    {
        if (replacing)
            buffer.copyFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, readPos), buffer.getNumSamples(), startGain, endGain);
        else
            buffer.addFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, readPos), buffer.getNumSamples(), startGain, endGain);
    }
    else
    {
        const auto midPos = delayBuffer.getNumSamples() - readPos;
        const auto midGain = juce::jmap(float(midPos) / buffer.getNumSamples(), startGain, endGain);
        if (replacing)
        {
            buffer.copyFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, readPos), midPos, startGain, midGain);
            buffer.copyFromWithRamp(channelOut, midPos, delayBuffer.getReadPointer(channelIn), buffer.getNumSamples() - midPos, midGain, endGain);
        }
        else
        {
            buffer.addFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, readPos), midPos, startGain, midGain);
            buffer.addFromWithRamp(channelOut, midPos, delayBuffer.getReadPointer(channelIn), buffer.getNumSamples() - midPos, midGain, endGain);
        }
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
void DigitalDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = tree.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DigitalDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(tree.state.getType()))
        {
            tree.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
    if (tree.state.hasProperty(getStepsParamName()))
        steps = tree.state.getProperty(getStepsParamName());
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