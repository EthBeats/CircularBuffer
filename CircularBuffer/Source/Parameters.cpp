/*
  ==============================================================================

    Parameters.cpp
    Created: 28 Jun 2024 5:06:07pm
    Author:  Ethan Miller

  ==============================================================================
*/

#include "Parameters.h"

template<typename T>
static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
	destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
	jassert(destination);
}

Parameters::Parameters(juce::AudioProcessorValueTreeState& apvts)
{
	castParameter(apvts, timeParamID, timeParam);
	castParameter(apvts, feedbackParamID, feedbackParam);
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createParameterLayout()
{
	juce::AudioProcessorValueTreeState::ParameterLayout layout;
	
	layout.add(std::make_unique<juce::AudioParameterInt>(
		timeParamID,
		"Delay Time",
		juce::NormalisableRange<float> { 0, 96000 },
		0));
		
	layout.add(std::make_unique<juce::AudioParameterFloat>(
		feedbackParamID,
		"Feedback",
		juce::NormalisableRange<float> { 0.0f, 1.0f },
		0.0f));
	
	return layout;
}

void Parameters::prepareToPlay(double sampleRate) noexcept
{
	double duration = 0.02;
	timeSmoother.reset(sampleRate, duration);
	feedbackSmoother.reset(sampleRate, duration);
}

void Parameters::reset() noexcept
{
	time = 0.0f;
	timeSmoother.setCurrentAndTargetValue(timeParam->get());
	
	feedback = 0.0f;
	feedbackSmoother.setCurrentAndTargetValue(feedbackParam->get());
}

void Parameters::update() noexcept
{
	timeSmoother.setTargetValue(timeParam->get());
	feedbackSmoother.setTargetValue(feedbackParam->get());
}

void Parameters::smoothen() noexcept
{
	time = timeSmoother.getNextValue();
	feedback = feedbackSmoother.getNextValue();
}
