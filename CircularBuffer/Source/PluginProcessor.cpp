/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CircularBufferAudioProcessor::CircularBufferAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	this->writePosition = 0;
}

CircularBufferAudioProcessor::~CircularBufferAudioProcessor()
{
}

//==============================================================================
const juce::String CircularBufferAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CircularBufferAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CircularBufferAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CircularBufferAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CircularBufferAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CircularBufferAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CircularBufferAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CircularBufferAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String CircularBufferAudioProcessor::getProgramName (int index)
{
    return {};
}

void CircularBufferAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void CircularBufferAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	auto delayBufferSize = sampleRate * 2.f;
	delayBuffer.setSize(getTotalNumOutputChannels(), static_cast<int>(delayBufferSize));
}

void CircularBufferAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CircularBufferAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CircularBufferAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // cleanup garbage...
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
	
	// numSamples local variables
	auto bufferSize = buffer.getNumSamples();
	auto delayBufferSize = delayBuffer.getNumSamples();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        
        fillDelayBuffer (channel, bufferSize, delayBufferSize, channelData);
        readFromDelayBuffer (channel, bufferSize, delayBufferSize, buffer);
    }
    
    // console values for testing...
//    DBG ("Delay Buffer Size: " << delayBufferSize);
//    DBG ("Buffer Size: " << bufferSize);
//    DBG ("Write Position: " << writePosition);
    
    
	// update write position and keep within bounds (wrap around)
	writePosition += bufferSize;
	writePosition %= delayBufferSize;
}

void CircularBufferAudioProcessor::fillDelayBuffer (int channel, int bufferSize, int delayBufferSize, float* channelData)
{
	// check if main buffer copies without needing to wrap
	if (delayBufferSize > bufferSize + writePosition)
	{
		// copy main buffer to delay buffer
		delayBuffer.copyFromWithRamp (channel, writePosition, channelData, bufferSize, 0.1f, 0.1f);
	}
	else
	{
		// determine how much space is left at the end of delay buffer
		auto numSamplesToEnd = delayBufferSize - writePosition;
			
		// copy that amount from main buffer to delay buffer
		delayBuffer.copyFromWithRamp (channel, writePosition, channelData, numSamplesToEnd, 0.1f, 0.1f);
			
		// calculate remaining contents
		auto numSamplesAtStart = bufferSize - numSamplesToEnd;
			
		// copy remaining contents to beginning of delay buffer
		delayBuffer.copyFromWithRamp (channel, 0, channelData + numSamplesToEnd, numSamplesAtStart, 0.1f, 0.1f);
	}
}

void CircularBufferAudioProcessor::readFromDelayBuffer (int channel, int bufferSize, int delayBufferSize, juce::AudioBuffer<float>& buffer)
{
	// goes sampleRate into the past
	auto readPosition = writePosition - getSampleRate();
        
	// check for negative index -> NO NO
	if (readPosition < 0)
	{
		readPosition += delayBufferSize;
	}
		
	// check if delayBuffer can be added to buffer without needing to wrap
	if (delayBufferSize > bufferSize + readPosition)
	{
		// add delay buffer to main buffer
		buffer.addFromWithRamp (channel, 0, delayBuffer.getReadPointer (channel, readPosition), bufferSize, 0.7f, 0.7f);
	}
	else
	{
		// determine how much space is left at the end of delay buffer
		auto numSamplesToEnd = delayBufferSize - readPosition;
			
		// add that amount from delay buffer to main buffer
		buffer.addFromWithRamp (channel, 0, delayBuffer.getReadPointer (channel, readPosition), numSamplesToEnd, 0.7f, 0.7f);
			
		// calculate remaining contents
		auto numSamplesAtStart = bufferSize - numSamplesToEnd;
			
		// add remaining contents to main buffer
		buffer.addFromWithRamp (channel, numSamplesToEnd, delayBuffer.getReadPointer (channel, 0), numSamplesAtStart, 0.7f, 0.7f);
	}
}

//==============================================================================
bool CircularBufferAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CircularBufferAudioProcessor::createEditor()
{
    return new CircularBufferAudioProcessorEditor (*this);
}

//==============================================================================
void CircularBufferAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void CircularBufferAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CircularBufferAudioProcessor();
}

