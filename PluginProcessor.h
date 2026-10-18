#pragma once
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "tools/MidiModPitchState.h"
#include "tools/PresetHandler.h"
#include "DrumFeatureDemonstrator.h"
#include "src/SampleLoadable.h"

//==============================================================================
class DrumFeatureDemonstratorAudioProcessor  : public juce::AudioProcessor, public dfd::SampleLoadable,
                                               private juce::ValueTree::Listener, private juce::AsyncUpdater
{
public:
    friend class DrumFeatureDemonstratorAudioProcessorEditor;
    //==============================================================================
    DrumFeatureDemonstratorAudioProcessor();
    ~DrumFeatureDemonstratorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // deploys the factory presets and reads the preset files (once; called before the first editor is created)
    void ensurePresetsLoaded();

    // message thread: reads the sample file and hands it to the audio thread; false if it cannot be read
    bool loadSample(const juce::File& file) override;
    juce::String getSampleName() const override;

    // the folder the last sample came from (kept in user.settings), the start folder of the file dialog
    juce::File getLastSampleFolder();
    void setLastSampleFolder(const juce::File& folder);

    float getScaleFactor(){return m_pluginScaleFactor;};
    void setScaleFactor(float newscalefactor){m_pluginScaleFactor = newscalefactor;};
    
    // Algo component and ValueTreeState must be public to be accessed by the editor
    DrumFeatureDemonstratorAudio m_algo;
    std::unique_ptr<AudioProcessorValueTreeState> m_parameterVTS;
private:
    // the sample name in the plugin state (property "SampleFile") follows the loaded sample, presets and DAW projects restore it
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree& tree) override; // replaceState() (preset or DAW state) replaces the whole tree
    void handleAsyncUpdate() override;
    juce::File getSamplesFolderOfUser();
    void loadSampleFromState();

    CriticalSection m_protect;
    float m_fs; // sampling rate is always needed

    //Parameterhandling
    std::vector <std::unique_ptr<RangedAudioParameter>> m_paramVector;
	PresetHandler m_presets;
    float m_pluginScaleFactor = 1.0;    
    juce::String m_sampleName;
    bool m_presetsLoaded = false; // see ensurePresetsLoaded()
    juce::String m_loadedSampleFileName; // name of the copy in the samples folder that is loaded now
#if WITH_MIDIKEYBOARD    
    MidiKeyboardState m_keyboardState;
    MidiModPitchBendState m_wheelState;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumFeatureDemonstratorAudioProcessor)
};
