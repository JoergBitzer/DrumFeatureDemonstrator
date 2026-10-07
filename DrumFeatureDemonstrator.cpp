#include <math.h>
#include "DrumFeatureDemonstrator.h"

#include "PluginProcessor.h"
#include "src/NoteNames.h"
#include "src/StepScheduler.h"
#include "src/SampleLoader.h"

DrumFeatureDemonstratorAudio::DrumFeatureDemonstratorAudio(DrumFeatureDemonstratorAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    m_soundStarts.reserve(dfd::kMaxSoundStartsPerBlock);
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

void DrumFeatureDemonstratorAudio::collectStepEntries(const juce::MidiBuffer& midiMessages)
{
    for (const juce::MidiMessageMetadata metadata : midiMessages)
    {
        const juce::MidiMessage message = metadata.getMessage();
        if (message.isNoteOn() == false)
        {
            continue;
        }
        int startIndex1 = 0;
        int blockSize1 = 0;
        int startIndex2 = 0;
        int blockSize2 = 0;
        m_stepEntryFifo.prepareToWrite(1, startIndex1, blockSize1, startIndex2, blockSize2);
        if (blockSize1 > 0)
        {
            m_stepEntries[static_cast<std::size_t>(startIndex1)] = message.getNoteNumber() + (message.getVelocity() << kVelocityShift);
            m_stepEntryFifo.finishedWrite(1);
        }
    }
}

bool DrumFeatureDemonstratorAudio::popStepEntry(int& noteNumber, int& velocity)
{
    int startIndex1 = 0;
    int blockSize1 = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;
    m_stepEntryFifo.prepareToRead(1, startIndex1, blockSize1, startIndex2, blockSize2);
    if (blockSize1 == 0)
    {
        return false;
    }
    const int entry = m_stepEntries[static_cast<std::size_t>(startIndex1)];
    m_stepEntryFifo.finishedRead(1);
    noteNumber = entry % (1 << kVelocityShift);
    velocity = entry >> kVelocityShift;
    return true;
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
    pattern.baseNote = static_cast<double>(m_baseNote->load());
    for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
    {
        dfd::StepSettings& step = dfd::stepAt(pattern, stepIndex);
        step.active = stepValue(stepIndex, dfd::StepParam::Active) > kHalf;
        step.noteNumber = static_cast<double>(stepValue(stepIndex, dfd::StepParam::Note));
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
    if (result.currentStep < 0)
    {
        collectStepEntries(midiMessages); // the transport stands: notes also set the selected step
    }

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
    paramVector.push_back(jade::makeParameter(g_paramDeclick));
    for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
    {
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Active)));
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Mode)));
        paramVector.push_back(jade::makeParameter(dfd::stepParameterSpec(stepIndex, dfd::StepParam::Note)));
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


namespace
{
constexpr int kTimerIntervalMs = 33;
constexpr int kSkippedNoticeTicks = 90; // about 3 seconds
constexpr float kRowFractionTop = 0.09f;
constexpr float kRowFractionSecond = 0.09f;
constexpr float kWaveformFraction = 0.22f;
constexpr float kLabelFractionOfRow = 0.62f;
constexpr float kFontFractionOfRow = 0.45f;
constexpr int kBarTextBoxWidth = 80;
constexpr int kBarTextBoxHeight = 20;
constexpr float kMargin = 0.006f;
const juce::Colour kNoticeColour(0xFFE05050);

juce::String noteNameOfMidiNote(int midiNote)
{
    return juce::String(dfd::noteName(midiNote));
}

void setupLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredRight);
}

void setupBarSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, kBarTextBoxWidth, kBarTextBoxHeight); // for the bar styles the text sits inside the bar
}
} // namespace

DrumFeatureDemonstratorGUI::DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts),
 m_waveform(*apvts.getParameter(g_paramRegionStart.ID), *apvts.getParameter(g_paramRegionEnd.ID)),
 m_stepGrid(apvts),
 m_declickAttachment(apvts, g_paramDeclick.ID, m_declickButton),
 m_syncAttachment(apvts, g_paramSyncToHost.ID, m_syncButton),
 m_playAttachment(apvts, g_paramInternalPlay.ID, m_playButton),
 m_baseNoteAttachment(apvts, g_paramBaseNote.ID, m_baseNoteSlider),
 m_lengthAttachment(apvts, g_paramPatternLength.ID, m_lengthSlider),
 m_bpmAttachment(apvts, g_paramInternalBpm.ID, m_bpmSlider)
{
    m_loadButton.setButtonText("Load sample");
    m_loadButton.setTooltip("Load a sample file (wav, aiff, flac, ogg). You can also drop a file onto the waveform.");
    m_loadButton.onClick = [this]() { chooseSampleFile(); };
    addAndMakeVisible(m_loadButton);

    m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    addAndMakeVisible(m_sampleLabel);

    m_declickButton.setButtonText("Declick");
    m_declickButton.setTooltip(jade::helpText(g_paramDeclick));
    addAndMakeVisible(m_declickButton);

    setupLabel(m_baseNoteLabel, "Base note");
    setupBarSlider(m_baseNoteSlider);
    m_baseNoteSlider.setTooltip(jade::helpText(g_paramBaseNote));
    m_baseNoteSlider.textFromValueFunction = [](double value) { return noteNameOfMidiNote(static_cast<int>(std::lround(value))); };
    m_baseNoteSlider.updateText();
    addAndMakeVisible(m_baseNoteLabel);
    addAndMakeVisible(m_baseNoteSlider);

    setupLabel(m_lengthLabel, "Pattern");
    setupBarSlider(m_lengthSlider);
    m_lengthSlider.setTooltip(jade::helpText(g_paramPatternLength));
    addAndMakeVisible(m_lengthLabel);
    addAndMakeVisible(m_lengthSlider);

    m_syncButton.setClickingTogglesState(true);
    m_syncButton.setTooltip(jade::helpText(g_paramSyncToHost));
    addAndMakeVisible(m_syncButton);

    m_playButton.setClickingTogglesState(true);
    m_playButton.setTooltip(jade::helpText(g_paramInternalPlay));
    addAndMakeVisible(m_playButton);

    setupLabel(m_tempoLabel, "Tempo");
    addAndMakeVisible(m_tempoLabel);
    m_hostBpmLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_hostBpmLabel);
    setupBarSlider(m_bpmSlider);
    m_bpmSlider.setTooltip(jade::helpText(g_paramInternalBpm));
    addAndMakeVisible(m_bpmSlider);

    m_infoLabel.setJustificationType(juce::Justification::centredLeft);
    m_infoLabel.setText("Hover over or turn a control to see its value here. Cursor keys or a click select a step; MIDI notes set its note and velocity while the transport stands.", juce::dontSendNotification);
    addAndMakeVisible(m_infoLabel);
    m_stepGrid.onInfo = [this](const juce::String& text) { showInfo(text); };

    m_noticeLabel.setJustificationType(juce::Justification::centredLeft);
    m_noticeLabel.setColour(juce::Label::textColourId, kNoticeColour);
    addAndMakeVisible(m_noticeLabel);

    m_waveform.onFileDropped = [this](const juce::File& file) { loadSampleFile(file); };
    addAndMakeVisible(m_waveform);
    addAndMakeVisible(m_stepGrid);

    int staleNote = 0;
    int staleVelocity = 0;
    while (m_processor.m_algo.popStepEntry(staleNote, staleVelocity))
    {
        // notes that arrived while no editor was open must not change a step now
    }
    m_lastSkippedCount = m_processor.m_algo.getSkippedSoundCount();
    updateTempoControls(m_syncButton.getToggleState());
    startTimerHz(1000 / kTimerIntervalMs);
    timerCallback();
}

DrumFeatureDemonstratorGUI::~DrumFeatureDemonstratorGUI()
{
    stopTimer();
}

void DrumFeatureDemonstratorGUI::updateTempoControls(bool syncToHost)
{
    m_syncButton.setButtonText("Host sync");
    if (syncToHost)
    {
        m_playButton.setButtonText("Host plays");
    }
    else if (m_playButton.getToggleState())
    {
        m_playButton.setButtonText("Pause");
    }
    else
    {
        m_playButton.setButtonText("Play");
    }
    m_playButton.setEnabled(syncToHost == false);
    m_hostBpmLabel.setVisible(syncToHost);
    m_bpmSlider.setVisible(syncToHost == false);
}

void DrumFeatureDemonstratorGUI::updateNotice()
{
    const juce::uint32 invalidMask = m_processor.m_algo.getInvalidStepMask();
    const int skippedCount = m_processor.m_algo.getSkippedSoundCount();
    if (skippedCount != m_lastSkippedCount)
    {
        m_lastSkippedCount = skippedCount;
        m_noticeTicksLeft = kSkippedNoticeTicks;
    }
    if (invalidMask != 0)
    {
        m_noticeLabel.setText("Play to end impossible: sample longer than the pattern.", juce::dontSendNotification);
    }
    else if (m_noticeTicksLeft > 0)
    {
        m_noticeLabel.setText("Sound skipped: its start time had already passed.", juce::dontSendNotification);
    }
    else
    {
        m_noticeLabel.setText("", juce::dontSendNotification);
    }
    if (m_noticeTicksLeft > 0)
    {
        --m_noticeTicksLeft;
    }
}

void DrumFeatureDemonstratorGUI::showInfo(const juce::String& text)
{
    if (text.isNotEmpty())
    {
        m_infoLabel.setText(text, juce::dontSendNotification);
    }
}

int DrumFeatureDemonstratorGUI::currentPatternLength() const
{
    return static_cast<int>(std::lround(m_lengthSlider.getValue()));
}

void DrumFeatureDemonstratorGUI::setParameterWithGesture(const juce::String& parameterId, float value)
{
    if (juce::RangedAudioParameter* parameter = m_apvts.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void DrumFeatureDemonstratorGUI::applyStepEntries()
{
    int note = 0;
    int velocity = 0;
    while (m_processor.m_algo.popStepEntry(note, velocity))
    {
        const int stepIndex = m_stepGrid.getSelectedStep();
        const float clampedNote = juce::jlimit(dfd::kMinStepNote, dfd::kMaxStepNote, static_cast<float>(note));
        const float clampedVelocity = juce::jlimit(dfd::kMinVelocity, dfd::kMaxVelocity, static_cast<float>(velocity));
        setParameterWithGesture(juce::String(dfd::stepParameterId(stepIndex, dfd::StepParam::Active)), 1.0f);
        setParameterWithGesture(juce::String(dfd::stepParameterId(stepIndex, dfd::StepParam::Note)), clampedNote);
        setParameterWithGesture(juce::String(dfd::stepParameterId(stepIndex, dfd::StepParam::Velocity)), clampedVelocity);
        showInfo("Step " + juce::String(stepIndex + 1) + " set by MIDI: note " + juce::String(dfd::noteName(static_cast<int>(clampedNote)))
                 + ", velocity " + juce::String(static_cast<int>(clampedVelocity)));
    }
}

bool DrumFeatureDemonstratorGUI::toggleInternalPlayer()
{
    if (m_syncButton.getToggleState())
    {
        showInfo("The space key starts the internal player; it is off while Host sync is on (the DAW plays).");
        return false; // the host may use the space key itself
    }
    const bool isPlaying = m_playButton.getToggleState();
    float newValue = 1.0f;
    if (isPlaying)
    {
        newValue = 0.0f;
    }
    setParameterWithGesture(juce::String(g_paramInternalPlay.ID), newValue);
    if (isPlaying)
    {
        showInfo("Internal player paused");
    }
    else
    {
        showInfo("Internal player started");
    }
    return true;
}

bool DrumFeatureDemonstratorGUI::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);
    constexpr int kStepsPerRowOfGrid = 16;
    const int length = currentPatternLength();
    if (key == juce::KeyPress::leftKey)
    {
        m_stepGrid.moveSelectedStep(-1, length);
    }
    else if (key == juce::KeyPress::rightKey)
    {
        m_stepGrid.moveSelectedStep(1, length);
    }
    else if (key == juce::KeyPress::upKey)
    {
        m_stepGrid.moveSelectedStep(-kStepsPerRowOfGrid, length);
    }
    else if (key == juce::KeyPress::downKey)
    {
        m_stepGrid.moveSelectedStep(kStepsPerRowOfGrid, length);
    }
    else if (key == juce::KeyPress::homeKey)
    {
        m_stepGrid.setSelectedStep(0);
    }
    else if (key == juce::KeyPress::endKey)
    {
        m_stepGrid.setSelectedStep(length - 1);
    }
    else if (key == juce::KeyPress::spaceKey)
    {
        return toggleInternalPlayer();
    }
    else
    {
        return false;
    }
    showInfo("Selected step " + juce::String(m_stepGrid.getSelectedStep() + 1));
    timerCallback();
    return true;
}

void DrumFeatureDemonstratorGUI::timerCallback()
{
    applyStepEntries();
    const bool syncToHost = m_syncButton.getToggleState();
    updateTempoControls(syncToHost);
    if (syncToHost)
    {
        if (m_processor.m_algo.isHostPositionAvailable())
        {
            m_hostBpmLabel.setText(juce::String(m_processor.m_algo.getDisplayedBpm(), 1) + " bpm (host)", juce::dontSendNotification);
        }
        else
        {
            m_hostBpmLabel.setText("host gives no beat position", juce::dontSendNotification);
        }
    }

    std::shared_ptr<const dfd::SampleData> sample = m_processor.m_algo.getSampleForGui();
    if (sample != m_shownSample)
    {
        m_shownSample = sample;
        m_waveform.setSample(sample);
    }
    if (m_loadMessage.isNotEmpty() && m_sampleLabel.getText() == m_loadMessage)
    {
        // keep the message of a failed load until the next load
    }
    else if (m_sampleLabel.getText() != m_processor.getSampleName())
    {
        m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    }
    m_waveform.setDeclickEnabled(m_declickButton.getToggleState());
    m_stepGrid.update(m_processor.m_algo.getCurrentStep(), m_processor.m_algo.getInvalidStepMask(),
                      static_cast<int>(std::lround(m_lengthSlider.getValue())));
    updateNotice();
}

bool DrumFeatureDemonstratorGUI::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() > 0;
}

void DrumFeatureDemonstratorGUI::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    if (files.size() == 0)
    {
        return;
    }
    // prefer a file with a known audio extension, otherwise try the first one (the loader knows better)
    for (const juce::String& path : files)
    {
        if (isSampleFileExtension(juce::File(path)))
        {
            loadSampleFile(juce::File(path));
            return;
        }
    }
    loadSampleFile(juce::File(files[0]));
}

bool DrumFeatureDemonstratorGUI::isSampleFileExtension(const juce::File& file)
{
    const juce::StringArray extensions{".wav", ".aif", ".aiff", ".flac", ".ogg"};
    return extensions.contains(file.getFileExtension().toLowerCase());
}

void DrumFeatureDemonstratorGUI::chooseSampleFile()
{
    m_fileChooser = std::make_unique<juce::FileChooser>("Load a sample", m_processor.getLastSampleFolder(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
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
    m_loadMessage = juce::String();
    m_processor.setLastSampleFolder(file.getParentDirectory()); // the next "Load sample" starts here
    if (m_processor.loadSample(file))
    {
        m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    }
    else
    {
        m_loadMessage = "Could not read " + file.getFileName() + " (use wav, aiff, flac or ogg)";
        m_sampleLabel.setText(m_loadMessage, juce::dontSendNotification);
    }
}

void DrumFeatureDemonstratorGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId).withAlpha(0.6f));
    g.setFont (juce::jmax(10.0f, 0.016f * static_cast<float>(getHeight())));
    juce::String text2display = "V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds(), juce::Justification::bottomRight, 1);
}

void DrumFeatureDemonstratorGUI::placeControlWithLabel(juce::Rectangle<int> area, juce::Label& label, juce::Component& control)
{
    label.setBounds(area.removeFromLeft(static_cast<int>(static_cast<float>(area.getWidth()) * kLabelFractionOfRow * 0.8f)));
    control.setBounds(area);
}

void DrumFeatureDemonstratorGUI::resized()
{
    auto r = getLocalBounds();
    const int margin = static_cast<int>(kMargin * static_cast<float>(r.getWidth()));
    r.reduce(margin, margin);
    const int height = getHeight();
    const int rowHeight = static_cast<int>(kRowFractionTop * static_cast<float>(height));
    const juce::Font rowFont(juce::FontOptions(kFontFractionOfRow * static_cast<float>(rowHeight)));
    for (juce::Label* label : {&m_sampleLabel, &m_baseNoteLabel, &m_lengthLabel, &m_tempoLabel, &m_hostBpmLabel, &m_noticeLabel, &m_infoLabel})
    {
        label->setFont(rowFont);
    }

    auto firstRow = r.removeFromTop(rowHeight);
    const int unit = firstRow.getWidth() / 20;
    m_loadButton.setBounds(firstRow.removeFromLeft(3 * unit).reduced(margin / 2, 0));
    m_sampleLabel.setBounds(firstRow.removeFromLeft(5 * unit));
    m_declickButton.setBounds(firstRow.removeFromLeft(3 * unit));
    placeControlWithLabel(firstRow.removeFromLeft(4 * unit).reduced(margin / 2, 0), m_baseNoteLabel, m_baseNoteSlider);
    placeControlWithLabel(firstRow.reduced(margin / 2, 0), m_lengthLabel, m_lengthSlider);

    r.removeFromTop(margin);
    auto secondRow = r.removeFromTop(rowHeight);
    m_syncButton.setBounds(secondRow.removeFromLeft(3 * unit).reduced(margin / 2, 0));
    m_playButton.setBounds(secondRow.removeFromLeft(3 * unit).reduced(margin / 2, 0));
    auto tempoArea = secondRow.removeFromLeft(5 * unit).reduced(margin / 2, 0);
    m_tempoLabel.setBounds(tempoArea.removeFromLeft(tempoArea.getWidth() / 3));
    m_hostBpmLabel.setBounds(tempoArea);
    m_bpmSlider.setBounds(tempoArea);
    m_noticeLabel.setBounds(secondRow);

    r.removeFromTop(margin);
    m_waveform.setBounds(r.removeFromTop(static_cast<int>(kWaveformFraction * static_cast<float>(height))));
    r.removeFromTop(margin);
    const int infoHeight = static_cast<int>(0.8f * static_cast<float>(rowHeight));
    m_infoLabel.setBounds(r.removeFromBottom(infoHeight));
    m_stepGrid.setBounds(r);
}
