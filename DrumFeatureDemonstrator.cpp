#include <math.h>
#include "DrumFeatureDemonstrator.h"

#include "PluginProcessor.h"
#include "src/StepScheduler.h"
#include "src/SampleLoader.h"

DrumFeatureDemonstratorAudio::DrumFeatureDemonstratorAudio(DrumFeatureDemonstratorAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    m_soundStarts.reserve(dfd::kMaxSteps);
}

void DrumFeatureDemonstratorAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock);
    m_hostSampleRate = sampleRate;
    int synchronblocksize;
    synchronblocksize = static_cast<int>(round(g_desired_blocksize_ms * sampleRate * 0.001)); // 0.001 to transform ms to seconds;
    if (g_forcePowerOf2)
    {
        int nextpowerof2 = int(log2(synchronblocksize))+1;
        synchronblocksize = int(pow(2,nextpowerof2));
    }
    prepareSynchronProcessing(max_channels,synchronblocksize);
    m_Latency = synchronblocksize;
    m_voiceBank.stopAll();

}

namespace
{
constexpr float kHalf = 0.5f;

int regionPosition(float fraction, int numSamples)
{
    const float clampedFraction = juce::jlimit(0.f, 1.f, fraction);
    return static_cast<int>(std::lround(clampedFraction * static_cast<float>(numSamples)));
}
} // namespace

void DrumFeatureDemonstratorAudio::setSample(std::shared_ptr<const dfd::SampleData> sample)
{
    m_sampleHolder.setSample(std::move(sample));
}

void DrumFeatureDemonstratorAudio::addNoteEvents(const juce::MidiBuffer& midiMessages, const dfd::SampleData& sample,
                                                 int regionStart, int regionEnd)
{
    const float baseNote = m_baseNote->load();
    for (const juce::MidiMessageMetadata metadata : midiMessages)
    {
        const juce::MidiMessage message = metadata.getMessage();
        if (message.isNoteOn() == false)
        {
            continue;
        }
        const double semitones = static_cast<double>(message.getNoteNumber()) - static_cast<double>(baseNote);
        dfd::VoiceStart voiceStart;
        voiceStart.sample = &sample;
        voiceStart.regionStart = regionStart;
        voiceStart.regionEnd = regionEnd;
        voiceStart.playbackRatio = dfd::playbackRatio(semitones, sample.sampleRate, m_hostSampleRate);
        voiceStart.reverse = false;
        voiceStart.gain = message.getFloatVelocity();
        m_voiceBank.addEvent(metadata.samplePosition, voiceStart);
    }
}

float DrumFeatureDemonstratorAudio::stepValue(int stepIndex, dfd::StepParam param) const
{
    const std::size_t slot = static_cast<std::size_t>(stepIndex * dfd::kNumParamsPerStep + static_cast<int>(param));
    return m_stepParameters[slot]->load();
}

void DrumFeatureDemonstratorAudio::readPattern(dfd::Pattern& pattern) const
{
    pattern.numSteps = static_cast<int>(std::lround(m_patternLength->load()));
    for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
    {
        dfd::StepSettings& step = dfd::stepAt(pattern, stepIndex);
        step.active = stepValue(stepIndex, dfd::StepParam::Active) > kHalf;
        step.pitchSemitones = static_cast<double>(stepValue(stepIndex, dfd::StepParam::Pitch));
        step.velocity = stepValue(stepIndex, dfd::StepParam::Velocity);
        const float mode = stepValue(stepIndex, dfd::StepParam::Mode);
        if (mode > dfd::kModeReverse + kHalf)
        {
            step.mode = dfd::StepMode::ReverseToEnd;
        }
        else if (mode > dfd::kModeForward + kHalf)
        {
            step.mode = dfd::StepMode::Reverse;
        }
        else
        {
            step.mode = dfd::StepMode::Forward;
        }
    }
}

dfd::BlockTransport DrumFeatureDemonstratorAudio::getBlockTransport(int numSamples, bool syncToHost)
{
    dfd::BlockTransport transport;
    if (syncToHost)
    {
        m_hostPositionAvailable = false;
        juce::AudioPlayHead* playHead = m_processor->getPlayHead();
        if (playHead == nullptr)
        {
            return transport;
        }
        const juce::Optional<juce::AudioPlayHead::PositionInfo> position = playHead->getPosition();
        if (position.hasValue() == false)
        {
            return transport;
        }
        const juce::Optional<double> ppq = position->getPpqPosition();
        const juce::Optional<double> bpm = position->getBpm();
        if (ppq.hasValue() == false || bpm.hasValue() == false)
        {
            return transport;
        }
        transport.isPlaying = position->getIsPlaying();
        transport.hasPosition = true;
        transport.positionPpq = *ppq;
        transport.bpm = *bpm;
        m_hostPositionAvailable = true;
        return transport;
    }
    m_internalClock.setBpm(static_cast<double>(m_internalBpm->load()));
    m_internalClock.setPlaying(m_internalPlay->load() > kHalf);
    transport = m_internalClock.getTransport();
    m_internalClock.advance(numSamples, m_hostSampleRate);
    return transport;
}

void DrumFeatureDemonstratorAudio::updateInvalidSteps(const dfd::Pattern& pattern, const dfd::SampleRegion& region,
                                                      double bpm)
{
    dfd::Transport timing;
    timing.bpm = bpm;
    timing.hostSampleRate = m_hostSampleRate;
    juce::uint32 mask = 0;
    for (int stepIndex = 0; stepIndex < dfd::numUsedSteps(pattern); ++stepIndex)
    {
        const dfd::StepSettings& step = dfd::stepAt(pattern, stepIndex);
        if (step.active == false || step.mode != dfd::StepMode::ReverseToEnd)
        {
            continue;
        }
        if (dfd::isPlayToEndAllowed(pattern, step, region, timing) == false)
        {
            mask |= (juce::uint32(1) << stepIndex);
        }
    }
    m_invalidStepMask = mask;
}

void DrumFeatureDemonstratorAudio::addSequencerEvents(const dfd::SampleData& sample, int regionStart, int regionEnd)
{
    for (const dfd::SoundStart& soundStart : m_soundStarts)
    {
        dfd::VoiceStart voiceStart;
        voiceStart.sample = &sample;
        voiceStart.regionStart = regionStart;
        voiceStart.regionEnd = regionEnd;
        voiceStart.playbackRatio = soundStart.playbackRatio;
        voiceStart.reverse = soundStart.reverse;
        voiceStart.gain = soundStart.gain;
        m_voiceBank.addEvent(soundStart.sampleOffsetInBlock, voiceStart);
    }
}

int DrumFeatureDemonstratorAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(NrOfBlocksSinceLastProcessBlock);
    buffer.clear();
    if (m_sampleHolder.updateActiveSample())
    {
        m_voiceBank.stopAll();
    }
    const dfd::SampleData* sample = m_sampleHolder.getActiveSample();

    dfd::Pattern pattern;
    readPattern(pattern);
    dfd::SampleRegion region;
    int regionStart = 0;
    int regionEnd = 0;
    if (sample != nullptr)
    {
        const int numSampleSamples = sample->numSamples();
        regionStart = regionPosition(m_regionStart->load(), numSampleSamples);
        regionEnd = regionPosition(m_regionEnd->load(), numSampleSamples);
        region.lengthInFileSamples = static_cast<double>(regionEnd - regionStart);
        region.fileSampleRate = sample->sampleRate;
    }

    const bool syncToHost = m_syncToHost->load() > kHalf;
    const dfd::BlockTransport transport = getBlockTransport(buffer.getNumSamples(), syncToHost);
    dfd::SequencerResult result;
    m_sequencer.process(pattern, region, m_hostSampleRate, transport, buffer.getNumSamples(), m_soundStarts, result);
    m_currentStep = result.currentStep;
    m_displayedBpm = static_cast<float>(transport.bpm);
    if (result.missedPlayToEnd > 0)
    {
        m_skippedSoundCount += result.missedPlayToEnd;
    }
    updateInvalidSteps(pattern, region, transport.bpm);

    if (sample == nullptr)
    {
        return 0;
    }
    addSequencerEvents(*sample, regionStart, regionEnd);
    addNoteEvents(midiMessages, *sample, regionStart, regionEnd);
    m_voiceBank.render(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    return 0;
}

void DrumFeatureDemonstratorAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // one line per parameter, made from its definition in DrumFeatureDemonstrator.h (see tools/ParameterSpec.h)
    paramVector.push_back(jade::makeParameter(g_paramBaseNote));
    paramVector.push_back(jade::makeParameter(g_paramRegionStart));
    paramVector.push_back(jade::makeParameter(g_paramRegionEnd));
    paramVector.push_back(jade::makeParameter(g_paramPatternLength));
    paramVector.push_back(jade::makeParameter(g_paramSyncToHost));
    paramVector.push_back(jade::makeParameter(g_paramInternalBpm));
    paramVector.push_back(jade::makeParameter(g_paramInternalPlay));
    for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
    {
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Active)));
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Mode)));
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Pitch)));
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Velocity)));
    }

}

void DrumFeatureDemonstratorAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_baseNote = vts->getRawParameterValue(g_paramBaseNote.ID);
    m_regionStart = vts->getRawParameterValue(g_paramRegionStart.ID);
    m_regionEnd = vts->getRawParameterValue(g_paramRegionEnd.ID);
    m_patternLength = vts->getRawParameterValue(g_paramPatternLength.ID);
    m_syncToHost = vts->getRawParameterValue(g_paramSyncToHost.ID);
    m_internalBpm = vts->getRawParameterValue(g_paramInternalBpm.ID);
    m_internalPlay = vts->getRawParameterValue(g_paramInternalPlay.ID);
    for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
    {
        for (int paramIndex = 0; paramIndex < dfd::kNumParamsPerStep; ++paramIndex)
        {
            const dfd::StepParam param = static_cast<dfd::StepParam>(paramIndex);
            const std::size_t slot = static_cast<std::size_t>(stepIndex * dfd::kNumParamsPerStep + paramIndex);
            m_stepParameters[slot] = vts->getRawParameterValue(dfd::stepParameterId(stepIndex, param));
        }
    }
}


DrumFeatureDemonstratorGUI::DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts)
{
    m_loadButton.setButtonText("Load sample");
    m_loadButton.onClick = [this]() { chooseSampleFile(); };
    addAndMakeVisible(m_loadButton);

    m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    addAndMakeVisible(m_sampleLabel);
}

bool DrumFeatureDemonstratorGUI::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const juce::String& path : files)
    {
        if (isSampleFileExtension(juce::File(path)))
        {
            return true;
        }
    }
    return false;
}

void DrumFeatureDemonstratorGUI::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    for (const juce::String& path : files)
    {
        if (isSampleFileExtension(juce::File(path)))
        {
            loadSampleFile(juce::File(path));
            return;
        }
    }
}

bool DrumFeatureDemonstratorGUI::isSampleFileExtension(const juce::File& file)
{
    const juce::StringArray extensions{".wav", ".aif", ".aiff", ".flac", ".ogg"};
    return extensions.contains(file.getFileExtension().toLowerCase());
}

void DrumFeatureDemonstratorGUI::chooseSampleFile()
{
    m_fileChooser = std::make_unique<juce::FileChooser>("Load a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    m_fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
    {
        const juce::File chosenFile = chooser.getResult();
        if (chosenFile.existsAsFile())
        {
            loadSampleFile(chosenFile);
        }
    });
}

void DrumFeatureDemonstratorGUI::loadSampleFile(const juce::File& file)
{
    if (m_processor.loadSample(file))
    {
        m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    }
    else
    {
        m_sampleLabel.setText("Could not read " + file.getFileName(), juce::dontSendNotification);
    }
}

void DrumFeatureDemonstratorGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (12.0f);
    
    juce::String text2display = "DrumFeatureDemonstrator V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds(), juce::Justification::bottomLeft, 1);

}

void DrumFeatureDemonstratorGUI::resized()
{
	auto r = getLocalBounds();
    
    // if you have to place several components, use scaleFactor
    //int width = r.getWidth();
	//float scaleFactor = float(width)/g_minGuiSize_x;

    const int buttonHeight = r.getHeight() / 6;
    auto topRow = r.removeFromTop(buttonHeight);
    m_loadButton.setBounds(topRow.removeFromLeft(r.getWidth() / 4));
    m_sampleLabel.setBounds(topRow);
}
