/*
  ==============================================================================

    Parameters.h
    Created: 28 Jun 2024 5:06:07pm
    Author:  Ethan Miller

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

const juce::ParameterID timeParamID { "time", 1 };
const juce::ParameterID feedbackParamID { "feedback", 1 };

class Parameters
{
public:
	Parameters(juce::AudioProcessorValueTreeState& apvts);
	
	static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
	
	void prepareToPlay(double sampleRate) noexcept;
	void reset() noexcept;
	void update() noexcept;
	void smoothen() noexcept;
	
	int time = 0.0f;
	float feedback = 0.0f;
	
private:
	juce::AudioParameterInt* timeParam;
	juce::LinearSmoothedValue<int> timeSmoother;
	
	juce::AudioParameterFloat* feedbackParam;
	juce::LinearSmoothedValue<float> feedbackSmoother;
};
