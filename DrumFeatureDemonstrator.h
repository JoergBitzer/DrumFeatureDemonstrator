#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "tools/SynchronBlockProcessor.h"
#include "src/SampleHolder.h"
#include "src/VoiceBank.h"
#include "src/Sequencer.h"
#include "src/PatternData.h"
#include "src/NoteValues.h"
#include "src/platform/XdndTarget.h"
#include "gui/WaveformView.h"
#include "gui/StepGrid.h"
#include "gui/PatternSelector.h"
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

// the note value of one step: a choice parameter, made in addParameter()
const char* const g_stepNoteValueId = "StepNoteValue";

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
	const std::string ID = "FadeIn";
	const std::string name = "Fade in";
	const std::string unitName = "s";
	const float minValue = 0.f;
	const float maxValue = 10.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 3;
	const bool logFrequency = false;
	const std::string help = "Fade-in time at the start of the played region (the top handle at the start line of the waveform).";
}g_paramFadeIn;

const struct
{
	const std::string ID = "FadeOut";
	const std::string name = "Fade out";
	const std::string unitName = "s";
	const float minValue = 0.f;
	const float maxValue = 10.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 3;
	const bool logFrequency = false;
	const std::string help = "Fade-out time at the end of the played region (the top handle at the end line of the waveform).";
}g_paramFadeOut;

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
	const float minValue = 24.f;  // C1
	const float maxValue = 72.f;  // C5
	const float defaultValue = 60.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "MIDI note at which the sample plays at its original speed (60 = C4); range C1 - C5.";
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
    int getCurrentStep() const { return m_currentStep.load(); }     // step in the playing pattern, -1 if nothing plays
    int getCurrentPattern() const { return m_currentPattern.load(); } // the pattern of the chain that plays, -1 if none
    bool isTransportRunning() const { return m_transportRunning.load(); }
    int getSkippedSoundCount() const { return m_skippedSoundCount.load(); } // counts up with every skipped sound
    juce::uint32 getInvalidStepMask(int patternIndex) const { return m_invalidStepMasks[static_cast<std::size_t>(patternIndex)].load(); } // bit n: step n cannot play to end
    float getDisplayedBpm() const { return m_displayedBpm.load(); }
    bool isHostPositionAvailable() const { return m_hostPositionAvailable.load(); }
    // MIDI notes that arrived while the transport stood (step entry): the GUI applies them to the selected step
    bool popStepEntry(int& noteNumber, int& velocity);
    std::shared_ptr<const dfd::SampleData> getSampleForGui() const { return m_sampleHolder.getCurrentSampleForGui(); }
    // the four patterns (not host parameters, saved as a blob in the state and in the presets)
    dfd::PatternData& getPatternData() { return m_patternData; }
    const dfd::PatternData& getPatternData() const { return m_patternData; }

private:
    void readBank(dfd::PatternBank& bank) const;
    dfd::BlockTransport getBlockTransport(int numSamples, bool syncToHost);
    void addSequencerEvents(const dfd::SampleData& sample, int regionStart, int regionEnd);
    void updateInvalidSteps(const dfd::PatternBank& bank, const dfd::SampleRegion& region, double bpm);
    void collectStepEntries(const juce::MidiBuffer& midiMessages);
    void addNoteEvents(const juce::MidiBuffer& midiMessages, const dfd::SampleData& sample, int regionStart, int regionEnd);

	DrumFeatureDemonstratorAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_hostSampleRate = 0.0;
    std::atomic<float>* m_baseNote = nullptr;
    std::atomic<float>* m_regionStart = nullptr;
    std::atomic<float>* m_regionEnd = nullptr;
    std::atomic<float>* m_fadeIn = nullptr;
    std::atomic<float>* m_fadeOut = nullptr;
    double m_fadeInSamples = 0.0;  // of the block being processed, in file samples
    double m_fadeOutSamples = 0.0;
    std::atomic<float>* m_patternLength = nullptr;
    std::atomic<float>* m_stepNoteValue = nullptr;
    std::atomic<float>* m_syncToHost = nullptr;
    std::atomic<float>* m_internalBpm = nullptr;
    std::atomic<float>* m_internalPlay = nullptr;
    dfd::PatternData m_patternData;
    dfd::PatternBank m_bank; // the patterns of the block being processed
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
    std::atomic<int> m_currentPattern{-1};
    std::atomic<bool> m_transportRunning{false};
    std::atomic<int> m_skippedSoundCount{0};
    std::array<std::atomic<juce::uint32>, dfd::kNumPatterns> m_invalidStepMasks{};
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
    void startDropTargetIfNeeded();
    void fileDroppedOnHostWindow(const std::string& droppedText);
    void applyStepEntries();
    void refreshPatternViews();
    void setParameterWithGesture(const juce::String& parameterId, float value);
    void showInfo(const juce::String& text);
    int currentPatternLength() const;
    int currentEditPattern() const;
    bool anyPlayingPatternHasInvalidSteps() const;
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
    juce::Label m_stepValueLabel;
    juce::ComboBox m_stepValueCombo;
    juce::TextButton m_syncButton;
    juce::TextButton m_playButton;
    juce::Label m_tempoLabel;
    juce::Label m_hostBpmLabel;
    juce::Slider m_bpmSlider;
    juce::Label m_noticeLabel;
    juce::Label m_infoLabel;
    dfd::WaveformView m_waveform;
    dfd::StepGrid m_stepGrid;
    dfd::PatternSelector m_patternSelector;
    juce::Label m_pageLabel;
    juce::TextButton m_pageNoteButton;
    juce::TextButton m_pageVelocityButton;
    juce::TextButton m_pagePanButton;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    juce::AudioProcessorValueTreeState::ButtonAttachment m_declickAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment m_syncAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment m_playAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_baseNoteAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_lengthAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment m_stepValueAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_bpmAttachment;

    std::shared_ptr<const dfd::SampleData> m_shownSample;
    int m_lastSkippedCount = 0;
    int m_noticeTicksLeft = 0;
    std::unique_ptr<dfd::XdndDropTarget> m_dropTarget; // Linux: file drops into a host window (see src/platform/XdndTarget.h)
    juce::String m_loadMessage; // message of the last failed load, shown in the sample label
    unsigned m_shownPatternLoadCount = 0; // the load count of the PatternData that the grid shows (a preset load changes it)
};
