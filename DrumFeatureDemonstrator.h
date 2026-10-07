#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "tools/ParameterSpec.h"
#include "PluginSettings.h"

class DrumFeatureDemonstratorAudioProcessor;

// This is how we define our parameter as globals to use it in the audio processor as well as in the editor.
// Everything about a parameter is defined here once; tools/ParameterSpec.h makes the parameter
// (jade::makeParameter) and its help text (jade::helpText, e.g. as tooltip) from it.
const struct
{
	const std::string ID = "ExampleID";
	const std::string name = "Example";
	const std::string unitName = "xyz";
	const float minValue = 1.f;
	const float maxValue = 2.f;
	const float defaultValue = 1.2f;
	const int numDecimalPlaces = 1;     // display precision (also the step size, here 0.1)
	const bool logFrequency = false;    // true: logarithmic range for frequencies (whole Hz)
	const std::string help = "An example parameter; replace it with your own.";
}g_paramExample;


class DrumFeatureDemonstratorAudio : public SynchronBlockProcessor
{
public:
    DrumFeatureDemonstratorAudio(DrumFeatureDemonstratorAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);
    
    // some necessary info for the host
    int getLatency(){return m_Latency;};

private:
	DrumFeatureDemonstratorAudioProcessor* m_processor;
    int m_Latency = 0;
};

class DrumFeatureDemonstratorGUI : public juce::Component
{
public:
	DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
	DrumFeatureDemonstratorAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts; 

};
