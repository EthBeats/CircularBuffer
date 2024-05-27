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
                       ), APVTS (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
	// Add listeners
	APVTS.addParameterListener("delayms", this);
	APVTS.addParameterListener("feedback", this);
	
	this->delayMs = 0.f;
	this->feedback = 0.f;
	this->writePosition = 0;
}

CircularBufferAudioProcessor::~CircularBufferAudioProcessor()
{
	// remove listeners
	APVTS.removeParameterListener("delayms", this);
	APVTS.removeParameterListener("feedback", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout CircularBufferAudioProcessor::createParameterLayout()
{
	std::vector< std::unique_ptr<juce::RangedAudioParameter> > params;
	
	// ID, name... lower, upper, initial values
	auto pDelayMs = std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "delayms", 1 }, "Delay Ms", 0.f, 96000.f, 0.f);
	auto pFeedback = std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "feedback", 1 }, "Feedback", 0.f, 1.f, 0.0f);
	
	// move ownership of pointer to params
	params.push_back(std::move(pDelayMs));
	params.push_back(std::move(pFeedback));
	
	// constructs parameter layout using iterators (range constructor)
	return { params.begin(), params.end() };
}

void CircularBufferAudioProcessor::parameterChanged (const juce::String &ParameterID, float newValue)
{
	delayMs.setTargetValue (APVTS.getRawParameterValue ("delayms")->load());
	feedback.setTargetValue (APVTS.getRawParameterValue ("feedback")->load());
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
	auto delayBufferSize = sampleRate * samplesPerBlock * 2.f;
	delayBuffer.setSize(getTotalNumOutputChannels(), static_cast<int>(delayBufferSize));
	
	delayMs.reset (sampleRate, 0.05f);
	feedback.reset (sampleRate, 0.05f);
	
	delayMs.setTargetValue (APVTS.getRawParameterValue ("delayms")->load());
	feedback.setTargetValue (APVTS.getRawParameterValue ("feedback")->load());
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

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // auto* channelData = buffer.getWritePointer (channel);
        
        fillDelayBuffer (buffer, channel);
        readFromDelayBuffer (buffer, delayBuffer, channel);
        fillDelayBuffer (buffer, channel);
    }

    updateBufferPositions (buffer, delayBuffer);
}

void CircularBufferAudioProcessor::fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel)
{
	// numSamples local variables
	auto bufferSize = buffer.getNumSamples();
	auto delayBufferSize = delayBuffer.getNumSamples();
	
	// check if main buffer copies without needing to wrap
	if (delayBufferSize > bufferSize + writePosition)
	{
		// copy main buffer to delay buffer
		delayBuffer.copyFrom (channel, writePosition, buffer.getWritePointer (channel), bufferSize);
	}
	else
	{
		// determine how much space is left at the end of delay buffer
		auto numSamplesToEnd = delayBufferSize - writePosition;
			
		// copy that amount from main buffer to delay buffer
		delayBuffer.copyFrom (channel, writePosition, buffer.getWritePointer (channel), numSamplesToEnd);
			
		// calculate remaining contents
		auto numSamplesAtStart = bufferSize - numSamplesToEnd;
			
		// copy remaining contents to beginning of delay buffer
		delayBuffer.copyFrom (channel, 0, buffer.getWritePointer (channel, numSamplesToEnd), numSamplesAtStart);
	}
}

void CircularBufferAudioProcessor::readFromDelayBuffer (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer, int channel)
{
	// numSamples local variables
	auto bufferSize = buffer.getNumSamples();
	auto delayBufferSize = delayBuffer.getNumSamples();
	
	// delayMs parameter
	auto readPosition = writePosition - (delayMs.getNextValue());
	
	// feedback parameter
	auto g = feedback.getNextValue();
        
	// check for negative index -> NO NO
	if (readPosition < 0)
	{
		readPosition += delayBufferSize;
	}
		
	// check if delayBuffer can be added to buffer without needing to wrap
	if (delayBufferSize > bufferSize + readPosition)
	{
		// add delay buffer to main buffer
		buffer.addFromWithRamp (channel, 0, delayBuffer.getReadPointer (channel, readPosition), bufferSize, g, g);
	}
	else
	{
		// determine how much space is left at the end of delay buffer
		auto numSamplesToEnd = delayBufferSize - readPosition;
			
		// add that amount from delay buffer to main buffer
		buffer.addFromWithRamp (channel, 0, delayBuffer.getReadPointer (channel, readPosition), numSamplesToEnd, g, g);
			
		// calculate remaining contents
		auto numSamplesAtStart = bufferSize - numSamplesToEnd;
			
		// add remaining contents to main buffer
		buffer.addFromWithRamp (channel, numSamplesToEnd, delayBuffer.getReadPointer (channel, 0), numSamplesAtStart, g, g);
	}
}

void CircularBufferAudioProcessor::updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer)
{
	// numSamples local variables
	auto bufferSize = buffer.getNumSamples();
	auto delayBufferSize = delayBuffer.getNumSamples();
	
	// advance and keep write position inbounds
	writePosition += bufferSize;
	writePosition %= delayBufferSize;
}

//==============================================================================
bool CircularBufferAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CircularBufferAudioProcessor::createEditor()
{
    // return new CircularBufferAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void CircularBufferAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // save params
   juce::MemoryOutputStream stream(destData, false);
   APVTS.state.writeToStream(stream);
}

void CircularBufferAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // load params
   auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
   
   jassert(tree.isValid());
   
   APVTS.state = tree;
   
   delayMs.setTargetValue (APVTS.getRawParameterValue ("delayms")->load());
   feedback.setTargetValue (APVTS.getRawParameterValue ("feedback")->load());
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CircularBufferAudioProcessor();
}

