#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "src/SampleHolder.h"
#include "src/VoiceBank.h"
#include "tools/ParameterSpec.h"
#include "PluginSettings.h"

class DrumFeatureDemonstratorAudioProcessor;

// This is how we define our parameter as globals to use it in the audio processor as well as in the editor.
// Everything about a parameter is defined here once; tools/ParameterSpec.h makes the parameter
// (jade::makeParameter) and its help text (jade::helpText, e.g. as tooltip) from it.
const struct
{
	const std::string ID = "BaseNote";
	const std::string name = "Base note";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 127.f;
	const float defaultValue = 60.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "MIDI note at which the sample plays at its original speed (60 = C4).";
}g_paramBaseNote;

const struct
{
	const std::string ID = "RegionStart";
	const std::string name = "Start";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 1.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 4;
	const bool logFrequency = false;
	const std::string help = "Start of the played region, as a fraction of the sample length.";
}g_paramRegionStart;

const struct
{
	const std::string ID = "RegionEnd";
	const std::string name = "End";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 1.f;
	const float defaultValue = 1.f;
	const int numDecimalPlaces = 4;
	const bool logFrequency = false;
	const std::string help = "End of the played region, as a fraction of the sample length.";
}g_paramRegionEnd;


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

    // message thread: hands a loaded sample to the audio thread
    void setSample(std::shared_ptr<const dfd::SampleData> sample);

private:
    void addNoteEvents(const juce::MidiBuffer& midiMessages, const dfd::SampleData& sample, int regionStart, int regionEnd);

	DrumFeatureDemonstratorAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_hostSampleRate = 0.0;
    std::atomic<float>* m_baseNote = nullptr;
    std::atomic<float>* m_regionStart = nullptr;
    std::atomic<float>* m_regionEnd = nullptr;
    dfd::SampleHolder m_sampleHolder;
    dfd::VoiceBank m_voiceBank;
};

class DrumFeatureDemonstratorGUI : public juce::Component, public juce::FileDragAndDropTarget
{
public:
	DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;

    // drop a sample file onto the GUI (the waveform view later)
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
private:
    static bool isSampleFileExtension(const juce::File& file);
    void chooseSampleFile();
    void loadSampleFile(const juce::File& file);

    juce::TextButton m_loadButton;
    juce::Label m_sampleLabel;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
	DrumFeatureDemonstratorAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts; 

};
