#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "src/SampleHolder.h"
#include "src/VoiceBank.h"
#include "src/Sequencer.h"
#include "src/StepParameters.h"
#include "gui/WaveformView.h"
#include "gui/StepGrid.h"
#include "tools/ParameterSpec.h"
#include "PluginSettings.h"
const struct
{
	const std::string ID = "PatternLength";
	const std::string name = "Pattern length";
	const std::string unitName = "steps";
	const float minValue = 8.f;
	const float maxValue = 32.f;
	const float defaultValue = 16.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Number of 1/16 steps of the pattern (8 - 32).";
}g_paramPatternLength;

const struct
{
	const std::string ID = "SyncToHost";
	const std::string name = "Sync to host";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 1.f;
	const float defaultValue = 1.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "1 = follow the tempo and position of the DAW, 0 = internal tempo and play button.";
}g_paramSyncToHost;

const struct
{
	const std::string ID = "InternalBpm";
	const std::string name = "Internal tempo";
	const std::string unitName = "bpm";
	const float minValue = 40.f;
	const float maxValue = 240.f;
	const float defaultValue = 120.f;
	const int numDecimalPlaces = 1;
	const bool logFrequency = false;
	const std::string help = "Tempo of the internal beat (used if Sync to host is off).";
}g_paramInternalBpm;

const struct
{
	const std::string ID = "InternalPlay";
	const std::string name = "Internal play";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 1.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Plays (1) or pauses (0) the internal beat (used if Sync to host is off).";
}g_paramInternalPlay;

const struct
{
	const std::string ID = "Declick";
	const std::string name = "Declick";
	const std::string unitName = "";
	const float minValue = 0.f;
	const float maxValue = 1.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "If on, the start and end handles snap to a zero crossing of the sample when you move them (no click at the region edges).";
}g_paramDeclick;


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

    // state for the GUI (written by the audio thread, read by the GUI timer)
    int getCurrentStep() const { return m_currentStep.load(); }
    int getSkippedSoundCount() const { return m_skippedSoundCount.load(); } // counts up with every skipped sound
    juce::uint32 getInvalidStepMask() const { return m_invalidStepMask.load(); } // bit n: step n cannot play to end
    float getDisplayedBpm() const { return m_displayedBpm.load(); }
    bool isHostPositionAvailable() const { return m_hostPositionAvailable.load(); }
    // MIDI notes that arrived while the transport stood (step entry): the GUI applies them to the selected step
    bool popStepEntry(int& noteNumber, int& velocity);
    std::shared_ptr<const dfd::SampleData> getSampleForGui() const { return m_sampleHolder.getCurrentSampleForGui(); }

private:
    void readPattern(dfd::Pattern& pattern) const;
    dfd::BlockTransport getBlockTransport(int numSamples, bool syncToHost);
    void addSequencerEvents(const dfd::SampleData& sample, int regionStart, int regionEnd);
    void updateInvalidSteps(const dfd::Pattern& pattern, const dfd::SampleRegion& region, double bpm);
    float stepValue(int stepIndex, dfd::StepParam param) const;
    void collectStepEntries(const juce::MidiBuffer& midiMessages);
    void addNoteEvents(const juce::MidiBuffer& midiMessages, const dfd::SampleData& sample, int regionStart, int regionEnd);

	DrumFeatureDemonstratorAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_hostSampleRate = 0.0;
    std::atomic<float>* m_baseNote = nullptr;
    std::atomic<float>* m_regionStart = nullptr;
    std::atomic<float>* m_regionEnd = nullptr;
    std::atomic<float>* m_patternLength = nullptr;
    std::atomic<float>* m_syncToHost = nullptr;
    std::atomic<float>* m_internalBpm = nullptr;
    std::atomic<float>* m_internalPlay = nullptr;
    std::array<std::atomic<float>*, dfd::kMaxSteps * dfd::kNumParamsPerStep> m_stepParameters{};
    static constexpr int kStepEntryCapacity = 32;
    static constexpr int kVelocityShift = 8; // an entry holds note + velocity * 256
    juce::AbstractFifo m_stepEntryFifo{kStepEntryCapacity};
    std::array<int, kStepEntryCapacity> m_stepEntries{};
    dfd::SampleHolder m_sampleHolder;
    dfd::VoiceBank m_voiceBank;
    dfd::Sequencer m_sequencer;
    dfd::InternalClock m_internalClock;
    std::vector<dfd::SoundStart> m_soundStarts;
    std::atomic<int> m_currentStep{-1};
    std::atomic<int> m_skippedSoundCount{0};
    std::atomic<juce::uint32> m_invalidStepMask{0};
    std::atomic<float> m_displayedBpm{0.f};
    std::atomic<bool> m_hostPositionAvailable{false};
};

class DrumFeatureDemonstratorGUI : public juce::Component, public juce::FileDragAndDropTarget, public juce::KeyListener, private juce::Timer
{
public:
	DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);
    ~DrumFeatureDemonstratorGUI() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

    // drop a sample file onto the GUI
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // cursor keys move the selected step (the editor forwards the keys it gets)
    using juce::Component::keyPressed;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
private:
    bool toggleInternalPlayer();
    void applyStepEntries();
    void setParameterWithGesture(const juce::String& parameterId, float value);
    void showInfo(const juce::String& text);
    int currentPatternLength() const;
    void timerCallback() override;
    static bool isSampleFileExtension(const juce::File& file);
    void chooseSampleFile();
    void loadSampleFile(const juce::File& file);
    void updateNotice();
    void updateTempoControls(bool syncToHost);
    void placeControlWithLabel(juce::Rectangle<int> area, juce::Label& label, juce::Component& control);

	DrumFeatureDemonstratorAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    juce::TextButton m_loadButton;
    juce::Label m_sampleLabel;
    juce::ToggleButton m_declickButton;
    juce::Label m_baseNoteLabel;
    juce::Slider m_baseNoteSlider;
    juce::Label m_lengthLabel;
    juce::Slider m_lengthSlider;
    juce::TextButton m_syncButton;
    juce::TextButton m_playButton;
    juce::Label m_tempoLabel;
    juce::Label m_hostBpmLabel;
    juce::Slider m_bpmSlider;
    juce::Label m_noticeLabel;
    juce::Label m_infoLabel;
    dfd::WaveformView m_waveform;
    dfd::StepGrid m_stepGrid;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    juce::AudioProcessorValueTreeState::ButtonAttachment m_declickAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment m_syncAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment m_playAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_baseNoteAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_lengthAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_bpmAttachment;

    std::shared_ptr<const dfd::SampleData> m_shownSample;
    int m_lastSkippedCount = 0;
    int m_noticeTicksLeft = 0;
    juce::String m_loadMessage; // message of the last failed load, shown in the sample label
};
