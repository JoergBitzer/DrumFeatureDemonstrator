#include "PluginProcessor.h"
#include "src/SampleLoader.h"
#include "src/SampleStore.h"
#include "tools/DayNightLookAndFeel.h"
#include "PluginEditor.h"

//==============================================================================
DrumFeatureDemonstratorAudioProcessor::DrumFeatureDemonstratorAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),m_algo(this), m_parameterVTS(nullptr)
{

    m_algo.addParameter(m_paramVector);

    m_parameterVTS = std::make_unique<AudioProcessorValueTreeState>(*this, nullptr, Identifier("DrumFeatureDemonstratorVTS"),
        AudioProcessorValueTreeState::ParameterLayout(m_paramVector.begin(), m_paramVector.end()));

    m_algo.prepareParameter(m_parameterVTS);
    m_parameterVTS->state.addListener(this);

	m_presets.setAudioValueTreeState(m_parameterVTS.get());
    // if needed add categories, if g_PresetCategories contains one empty string "", nothing happened
    m_presets.addCategory(g_PresetCategories);

#ifdef FACTORY_PRESETS    
    m_presets.DeployFactoryPresets();
#endif
    
	m_presets.loadfromFileAllUserPresets();    

    setLatencySamples(m_algo.getLatency());
}

DrumFeatureDemonstratorAudioProcessor::~DrumFeatureDemonstratorAudioProcessor()
{
    cancelPendingUpdate();
    m_parameterVTS->state.removeListener(this);
}

//==============================================================================
const juce::String DrumFeatureDemonstratorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DrumFeatureDemonstratorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DrumFeatureDemonstratorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DrumFeatureDemonstratorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}


double DrumFeatureDemonstratorAudioProcessor::getTailLengthSeconds() const
{

    return 0.0;
}

int DrumFeatureDemonstratorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DrumFeatureDemonstratorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DrumFeatureDemonstratorAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String DrumFeatureDemonstratorAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void DrumFeatureDemonstratorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void DrumFeatureDemonstratorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    // sometimes you need the number of channels.
    // Since we only support if in and output is the same, we can just ask for input
    // 
    int nrofchannels = this->getMainBusNumOutputChannels();
    jassert(nrofchannels > 0); // number of channels should never be zero

    juce::ignoreUnused (samplesPerBlock);
    m_fs = static_cast<float>(sampleRate);
    m_algo.prepareToPlay(sampleRate,samplesPerBlock,nrofchannels);
}

void DrumFeatureDemonstratorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool DrumFeatureDemonstratorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
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

void DrumFeatureDemonstratorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
 #if WITH_MIDIKEYBOARD  
	m_keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    m_wheelState.processNextMidiBuffer(midiMessages,true);
#else
    juce::ignoreUnused (midiMessages);
#endif

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    m_algo.processBlock(buffer,midiMessages);

#if WITH_MIDIKEYBOARD  
    midiMessages.clear(); // except you want to create new midi messages, but than say so 
    // by setting NEEDS_MIDI_OUTPUT in CMakeLists.txt
#endif
}

//==============================================================================
namespace
{
const char* const kSampleFileProperty = "SampleFile";
const char* const kSampleDataChild = "SampleData";
const char* const kSampleDataNameProperty = "name";
const char* const kSampleDataBytesProperty = "bytes";
const char* const kMissingPrefix = "Missing: ";
} // namespace

juce::File DrumFeatureDemonstratorAudioProcessor::getSamplesFolderOfUser()
{
    bool presetFolderWasCreated;
    return dfd::getSamplesFolder(m_presets.getUserPresetsFolder(presetFolderWasCreated));
}

bool DrumFeatureDemonstratorAudioProcessor::loadSample(const juce::File& file)
{
    std::shared_ptr<const dfd::SampleData> sample = dfd::loadSampleFile(file);
    if (sample == nullptr)
    {
        return false;
    }
    const juce::File storedCopy = dfd::storeSampleCopy(file, getSamplesFolderOfUser());
    m_algo.setSample(sample);
    m_sampleName = storedCopy.getFileName();
    m_loadedSampleFileName = storedCopy.getFileName();
    m_parameterVTS->state.setProperty(kSampleFileProperty, m_loadedSampleFileName, nullptr);
    return true;
}

void DrumFeatureDemonstratorAudioProcessor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == m_parameterVTS->state && property == juce::Identifier(kSampleFileProperty))
    {
        triggerAsyncUpdate();
    }
}

void DrumFeatureDemonstratorAudioProcessor::valueTreeRedirected(juce::ValueTree& tree)
{
    if (tree == m_parameterVTS->state)
    {
        triggerAsyncUpdate();
    }
}

void DrumFeatureDemonstratorAudioProcessor::handleAsyncUpdate()
{
    loadSampleFromState();
}

void DrumFeatureDemonstratorAudioProcessor::loadSampleFromState()
{
    const juce::String wantedName = m_parameterVTS->state.getProperty(kSampleFileProperty).toString();
    if (wantedName == m_loadedSampleFileName)
    {
        return;
    }
    if (wantedName.isEmpty())
    {
        // a preset without a sample keeps the current sample: write its name back into the state
        m_parameterVTS->state.setProperty(kSampleFileProperty, m_loadedSampleFileName, nullptr);
        return;
    }
    const juce::File wantedFile = getSamplesFolderOfUser().getChildFile(wantedName);
    std::shared_ptr<const dfd::SampleData> sample;
    if (wantedFile.existsAsFile())
    {
        sample = dfd::loadSampleFile(wantedFile);
    }
    if (sample == nullptr)
    {
        m_sampleName = kMissingPrefix + wantedName;
        return;
    }
    m_algo.setSample(sample);
    m_sampleName = wantedName;
    m_loadedSampleFileName = wantedName;
}

namespace
{
const char* const kLastSampleFolderProperty = "lastSampleFolder";
}

juce::File DrumFeatureDemonstratorAudioProcessor::getLastSampleFolder()
{
    bool presetFolderWasCreated;
    auto settings = jade::createUserSettings(m_presets.getUserPresetsFolder(presetFolderWasCreated));
    const juce::File folder(settings->getValue(kLastSampleFolderProperty));
    if (folder.isDirectory())
    {
        return folder;
    }
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory);
}

void DrumFeatureDemonstratorAudioProcessor::setLastSampleFolder(const juce::File& folder)
{
    bool presetFolderWasCreated;
    auto settings = jade::createUserSettings(m_presets.getUserPresetsFolder(presetFolderWasCreated));
    settings->setValue(kLastSampleFolderProperty, folder.getFullPathName());
    settings->saveIfNeeded();
}

juce::String DrumFeatureDemonstratorAudioProcessor::getSampleName() const
{
    return m_sampleName;
}

//==============================================================================
bool DrumFeatureDemonstratorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DrumFeatureDemonstratorAudioProcessor::createEditor()
{
    return new DrumFeatureDemonstratorAudioProcessorEditor (*this);
}

//==============================================================================
void DrumFeatureDemonstratorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
	auto state = m_parameterVTS->copyState();
    ValueTree vtpluginsize("PluginSize");
    vtpluginsize.setProperty("ScaleFactor",m_pluginScaleFactor,nullptr);
    state.appendChild(vtpluginsize,nullptr);

    // the sample itself goes into the state of the DAW project (not into presets), so a project works without the samples folder
    const juce::File sampleFile = getSamplesFolderOfUser().getChildFile(m_loadedSampleFileName);
    const juce::String sampleBytes = dfd::encodeFileAsBase64(sampleFile);
    if (m_loadedSampleFileName.isNotEmpty() && sampleBytes.isNotEmpty())
    {
        ValueTree vtsample(kSampleDataChild);
        vtsample.setProperty(kSampleDataNameProperty, m_loadedSampleFileName, nullptr);
        vtsample.setProperty(kSampleDataBytesProperty, sampleBytes, nullptr);
        state.appendChild(vtsample, nullptr);
    }

	std::unique_ptr<XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);

}

void DrumFeatureDemonstratorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
 	std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(m_parameterVTS->state.getType()))
        {
            ValueTree vt = ValueTree::fromXml(*xmlState);
            ValueTree subvt = vt.getChildWithName("PluginSize");
            if (subvt.isValid())
            {
                float val = subvt.getProperty("ScaleFactor");
                m_pluginScaleFactor = val;
                vt.removeChild(subvt, nullptr);

            }
            ValueTree vtsample = vt.getChildWithName(kSampleDataChild);
            if (vtsample.isValid())
            {
                const juce::String sampleName = vtsample.getProperty(kSampleDataNameProperty).toString();
                const juce::File sampleFile = getSamplesFolderOfUser().getChildFile(sampleName);
                dfd::restoreFileFromBase64(vtsample.getProperty(kSampleDataBytesProperty).toString(), sampleFile);
                vt.removeChild(vtsample, nullptr);
            }
            juce::String presetname(xmlState->getStringAttribute("presetname"));
            m_presets.setCurrentPresetName(presetname);

			m_parameterVTS->replaceState(vt);
        }

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumFeatureDemonstratorAudioProcessor();
}
