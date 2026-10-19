#include <math.h>
#include "DrumFeatureDemonstrator.h"

#include "PluginProcessor.h"
#include "src/NoteNames.h"
#include "src/DebugLog.h"
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
        voiceStart.fadeInSamples = m_fadeInSamples;
        voiceStart.fadeOutSamples = m_fadeOutSamples;
        voiceStart.playbackRatio = dfd::playbackRatio(semitones, sample.sampleRate, m_hostSampleRate);
        voiceStart.reverse = false;
        voiceStart.gain = message.getFloatVelocity();
        m_voiceBank.addEvent(metadata.samplePosition, voiceStart);
    }
}

void DrumFeatureDemonstratorAudio::readBank(dfd::PatternBank& bank) const
{
    bank.numSteps = static_cast<int>(std::lround(m_patternData.getSetting(dfd::PatternSetting::Length)));
    bank.baseNote = static_cast<double>(m_baseNote->load());
    bank.stepLengthInBeats = dfd::noteValueLengthInBeats(static_cast<int>(std::lround(m_patternData.getSetting(dfd::PatternSetting::NoteValue))));
    m_patternData.fillBank(bank);
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
    m_internalClock.setPlaying(m_internalPlay.load());
    transport = m_internalClock.getTransport();
    m_internalClock.advance(numSamples, m_hostSampleRate);
    return transport;
}

void DrumFeatureDemonstratorAudio::updateInvalidSteps(const dfd::PatternBank& bank, const dfd::SampleRegion& region,
                                                      double bpm)
{
    dfd::Transport timing;
    timing.bpm = bpm;
    timing.hostSampleRate = m_hostSampleRate;
    for (int patternIndex = 0; patternIndex < dfd::kNumPatterns; ++patternIndex)
    {
        juce::uint32 mask = 0;
        for (int stepIndex = 0; stepIndex < dfd::numUsedSteps(bank); ++stepIndex)
        {
            const dfd::StepSettings& step = dfd::stepAt(bank, patternIndex, stepIndex);
            if (step.active == false || step.mode != dfd::StepMode::ReverseToEnd)
            {
                continue;
            }
            if (dfd::isPlayToEndAllowed(bank, step, region, timing) == false)
            {
                mask |= (juce::uint32(1) << stepIndex);
            }
        }
        m_invalidStepMasks[static_cast<std::size_t>(patternIndex)] = mask;
    }
}

void DrumFeatureDemonstratorAudio::addSequencerEvents(const dfd::SampleData& sample, int regionStart, int regionEnd)
{
    for (const dfd::SoundStart& soundStart : m_soundStarts)
    {
        dfd::VoiceStart voiceStart;
        voiceStart.sample = &sample;
        voiceStart.regionStart = regionStart;
        voiceStart.regionEnd = regionEnd;
        voiceStart.fadeInSamples = m_fadeInSamples;
        voiceStart.fadeOutSamples = m_fadeOutSamples;
        voiceStart.playbackRatio = soundStart.playbackRatio;
        voiceStart.pan = soundStart.pan;
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

    readBank(m_bank);
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
        m_fadeInSamples = static_cast<double>(m_fadeIn->load()) * sample->sampleRate;
        m_fadeOutSamples = static_cast<double>(m_fadeOut->load()) * sample->sampleRate;
        const double totalFade = m_fadeInSamples + m_fadeOutSamples;
        if (totalFade > region.lengthInFileSamples && totalFade > 0.0)
        {
            // both fades together cannot be longer than the region: shorten them in proportion
            const double scale = region.lengthInFileSamples / totalFade;
            m_fadeInSamples *= scale;
            m_fadeOutSamples *= scale;
        }
    }

    const bool syncToHost = m_patternData.getSetting(dfd::PatternSetting::SyncToHost) > kHalf;
    const dfd::BlockTransport transport = getBlockTransport(buffer.getNumSamples(), syncToHost);
    dfd::SequencerResult result;
    m_sequencer.process(m_bank, region, m_hostSampleRate, transport, buffer.getNumSamples(), m_soundStarts, result);
    m_currentStep = result.currentStep;
    m_currentPattern = result.currentPattern;
    m_transportRunning = result.transportRunning;
    m_displayedBpm = static_cast<float>(transport.bpm);
    if (result.missedPlayToEnd > 0)
    {
        m_skippedSoundCount += result.missedPlayToEnd;
    }
    updateInvalidSteps(m_bank, region, transport.bpm);
    if (result.transportRunning == false)
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
    paramVector.push_back(jade::makeParameter(g_paramInternalBpm));
    paramVector.push_back(jade::makeParameter(g_paramFadeIn));
    paramVector.push_back(jade::makeParameter(g_paramFadeOut));
}

void DrumFeatureDemonstratorAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_baseNote = vts->getRawParameterValue(g_paramBaseNote.ID);
    m_regionStart = vts->getRawParameterValue(g_paramRegionStart.ID);
    m_regionEnd = vts->getRawParameterValue(g_paramRegionEnd.ID);
    m_fadeIn = vts->getRawParameterValue(g_paramFadeIn.ID);
    m_fadeOut = vts->getRawParameterValue(g_paramFadeOut.ID);
    m_internalBpm = vts->getRawParameterValue(g_paramInternalBpm.ID);
}


namespace
{
constexpr int kTimerIntervalMs = 33;
constexpr int kSkippedNoticeTicks = 90; // about 3 seconds
constexpr float kRowFractionTop = 0.09f;
constexpr float kRowFractionSecond = 0.09f;
constexpr float kWaveformFraction = 0.20f;
constexpr float kPatternStripFraction = 0.085f;
constexpr float kPatternStripWidthFraction = 0.30f;
constexpr float kBottomBarFraction = 0.065f;
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

// 1 if the button is switched on, 0 if not (the value of an on/off setting)
float switchValue(const juce::Button& button)
{
    if (button.getToggleState())
    {
        return 1.0f;
    }
    return 0.0f;
}

void setupBarSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, kBarTextBoxWidth, kBarTextBoxHeight); // for the bar styles the text sits inside the bar
}
} // namespace

DrumFeatureDemonstratorGUI::DrumFeatureDemonstratorGUI(DrumFeatureDemonstratorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts),
 m_waveform(*apvts.getParameter(g_paramRegionStart.ID), *apvts.getParameter(g_paramRegionEnd.ID),
            *apvts.getParameter(g_paramFadeIn.ID), *apvts.getParameter(g_paramFadeOut.ID)),
 m_stepGrid(p.m_algo.getPatternData()),
 m_patternSelector(p.m_algo.getPatternData()),
 m_baseNoteAttachment(apvts, g_paramBaseNote.ID, m_baseNoteSlider),
 m_bpmAttachment(apvts, g_paramInternalBpm.ID, m_bpmSlider)
{
    m_loadButton.setButtonText("Load sample");
    m_loadButton.setTooltip("Load a sample file (wav, aiff, flac, ogg). You can also drop a file onto the waveform.");
    m_loadButton.onClick = [this]() { chooseSampleFile(); };
    addAndMakeVisible(m_loadButton);

    m_sampleLabel.setText(m_processor.getSampleName(), juce::dontSendNotification);
    addAndMakeVisible(m_sampleLabel);

    m_declickButton.setButtonText("Declick");
    m_declickButton.setTooltip(dfd::patternSettingSpec(dfd::PatternSetting::Declick).help);
    m_declickButton.onClick = [this]() { setPatternSetting(dfd::PatternSetting::Declick, switchValue(m_declickButton)); };
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
    m_lengthSlider.setTooltip(dfd::patternSettingSpec(dfd::PatternSetting::Length).help);
    m_lengthSlider.setRange(static_cast<double>(dfd::kMinSteps), static_cast<double>(dfd::kMaxSteps), 1.0);
    m_lengthSlider.setTextValueSuffix(" steps");
    m_lengthSlider.onValueChange = [this]() { setPatternSetting(dfd::PatternSetting::Length, static_cast<float>(m_lengthSlider.getValue())); };
    addAndMakeVisible(m_lengthLabel);
    addAndMakeVisible(m_lengthSlider);

    setupLabel(m_stepValueLabel, "Step");
    for (int index = 0; index < dfd::kNumNoteValues; ++index)
    {
        m_stepValueCombo.addItem(juce::String(dfd::noteValueName(index)), index + 1);
    }
    m_stepValueCombo.setTooltip(dfd::patternSettingSpec(dfd::PatternSetting::NoteValue).help);
    m_stepValueCombo.onChange = [this]() { setPatternSetting(dfd::PatternSetting::NoteValue, static_cast<float>(m_stepValueCombo.getSelectedItemIndex())); };
    addAndMakeVisible(m_stepValueLabel);
    addAndMakeVisible(m_stepValueCombo);

    m_syncButton.setClickingTogglesState(true);
    m_syncButton.setTooltip(dfd::patternSettingSpec(dfd::PatternSetting::SyncToHost).help);
    m_syncButton.onClick = [this]() { setPatternSetting(dfd::PatternSetting::SyncToHost, switchValue(m_syncButton)); };
    addAndMakeVisible(m_syncButton);

    m_playButton.setClickingTogglesState(true);
    m_playButton.setTooltip("Plays (on) or pauses (off) the internal beat (used if Host sync is off). The space key does the same.");
    m_playButton.onClick = [this]() { m_processor.m_algo.setInternalPlay(m_playButton.getToggleState()); };
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
    m_stepGrid.onPatternDataEdited = [this]() { m_processor.commitPatternData(); };
    m_patternSelector.onPlaySwitchesEdited = [this]() { m_processor.commitPatternData(); };
    showPatternSettings();

    // the pattern buttons: the grid shows the edited pattern
    m_patternSelector.onEditPatternChanged = [this](int patternIndex) { m_stepGrid.setEditPattern(patternIndex); };
    m_stepGrid.setEditPattern(m_patternSelector.getEditPattern());
    addAndMakeVisible(m_patternSelector);

    // the pages of the bars under the steps
    constexpr int kPageRadioGroup = 2001;
    setupLabel(m_pageLabel, "Bars");
    addAndMakeVisible(m_pageLabel);
    const std::pair<juce::TextButton*, juce::String> pageButtons[] = {{&m_pageNoteButton, "Note"}, {&m_pageVelocityButton, "Velocity"}, {&m_pagePanButton, "Pan"}};
    for (const auto& entry : pageButtons)
    {
        entry.first->setButtonText(entry.second);
        entry.first->setClickingTogglesState(true);
        entry.first->setRadioGroupId(kPageRadioGroup);
        entry.first->setTooltip("The bars under the steps show the " + entry.second.toLowerCase() + " of every step.");
        addAndMakeVisible(*entry.first);
    }
    m_pageNoteButton.setToggleState(true, juce::dontSendNotification);
    m_pageNoteButton.onClick = [this]() { m_stepGrid.setPage(dfd::GridPage::Note); };
    m_pageVelocityButton.onClick = [this]() { m_stepGrid.setPage(dfd::GridPage::Velocity); };
    m_pagePanButton.onClick = [this]() { m_stepGrid.setPage(dfd::GridPage::Pan); };

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
    if (m_dropTarget != nullptr)
    {
        m_dropTarget->stop(); // the callback must not run any more
    }
}

void DrumFeatureDemonstratorGUI::startDropTargetIfNeeded()
{
#if JUCE_LINUX
    if (m_dropTarget != nullptr)
    {
        return;
    }
    juce::ComponentPeer* peer = getPeer();
    if (peer == nullptr)
    {
        return;
    }
    // a host that embeds the editor in its own window does not accept file drops for it: take them ourselves
    const unsigned long windowId = reinterpret_cast<unsigned long>(peer->getNativeHandle());
    m_dropTarget = std::make_unique<dfd::XdndDropTarget>();
    juce::Component::SafePointer<DrumFeatureDemonstratorGUI> safeThis(this);
    m_dropTarget->start(windowId, [safeThis](const std::string& droppedText)
    {
        // called from the thread of the drop target: continue on the message thread
        juce::MessageManager::callAsync([safeThis, droppedText]()
        {
            if (safeThis != nullptr)
            {
                safeThis->fileDroppedOnHostWindow(droppedText);
            }
        });
    });
    dfd::debugLog("drop target started for window " + juce::String(static_cast<juce::int64>(windowId)));
#endif
}

void DrumFeatureDemonstratorGUI::fileDroppedOnHostWindow(const std::string& droppedText)
{
    dfd::debugLog("drop on the host window: " + juce::String(droppedText));
    juce::String path = juce::String(droppedText).trim();
    const juce::String filePrefix("file://");
    if (path.startsWithIgnoreCase(filePrefix))
    {
        path = juce::URL::removeEscapeChars(path.substring(filePrefix.length()));
        const juce::String localhostPrefix("localhost");
        if (path.startsWith(localhostPrefix))
        {
            path = path.substring(localhostPrefix.length());
        }
    }
    loadSampleFile(juce::File(path));
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

int DrumFeatureDemonstratorGUI::currentEditPattern() const
{
    return m_patternSelector.getEditPattern();
}

// play to end is impossible for a step of a pattern that is switched on
bool DrumFeatureDemonstratorGUI::anyPlayingPatternHasInvalidSteps() const
{
    for (int patternIndex = 0; patternIndex < dfd::kNumPatterns; ++patternIndex)
    {
        if (m_processor.m_algo.getPatternData().isPlaying(patternIndex) && m_processor.m_algo.getInvalidStepMask(patternIndex) != 0)
        {
            return true;
        }
    }
    return false;
}

void DrumFeatureDemonstratorGUI::updateNotice()
{
    const bool anyInvalid = anyPlayingPatternHasInvalidSteps();
    const int skippedCount = m_processor.m_algo.getSkippedSoundCount();
    if (skippedCount != m_lastSkippedCount)
    {
        m_lastSkippedCount = skippedCount;
        m_noticeTicksLeft = kSkippedNoticeTicks;
    }
    if (anyInvalid)
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

// the controls show the settings of the PatternData (without sending a change back)
void DrumFeatureDemonstratorGUI::showPatternSettings()
{
    const dfd::PatternData& patternData = m_processor.m_algo.getPatternData();
    m_shownPatternLoadCount = patternData.getLoadCount();
    m_lengthSlider.setValue(static_cast<double>(patternData.getSetting(dfd::PatternSetting::Length)), juce::dontSendNotification);
    const int noteValueIndex = static_cast<int>(std::lround(patternData.getSetting(dfd::PatternSetting::NoteValue)));
    m_stepValueCombo.setSelectedItemIndex(dfd::clampedNoteValueIndex(noteValueIndex), juce::dontSendNotification);
    m_syncButton.setToggleState(patternData.getSetting(dfd::PatternSetting::SyncToHost) > kHalf, juce::dontSendNotification);
    m_declickButton.setToggleState(patternData.getSetting(dfd::PatternSetting::Declick) > kHalf, juce::dontSendNotification);
}

void DrumFeatureDemonstratorGUI::setPatternSetting(dfd::PatternSetting setting, float value)
{
    m_processor.m_algo.getPatternData().setSetting(setting, value);
    m_processor.commitPatternData();
}

// a preset or a DAW state replaced the pattern data: the grid, the pattern buttons and the settings show the new values
void DrumFeatureDemonstratorGUI::refreshPatternViews()
{
    const unsigned loadCount = m_processor.m_algo.getPatternData().getLoadCount();
    if (loadCount != m_shownPatternLoadCount)
    {
        m_stepGrid.refreshFromData();
        m_patternSelector.refresh();
        showPatternSettings();
    }
}

void DrumFeatureDemonstratorGUI::applyStepEntries()
{
    int note = 0;
    int velocity = 0;
    while (m_processor.m_algo.popStepEntry(note, velocity))
    {
        const int stepIndex = m_stepGrid.getSelectedStep();
        const int patternIndex = currentEditPattern();
        const float clampedNote = juce::jlimit(dfd::kMinStepNote, dfd::kMaxStepNote, static_cast<float>(note));
        const float clampedVelocity = juce::jlimit(dfd::kMinVelocity, dfd::kMaxVelocity, static_cast<float>(velocity));
        dfd::PatternData& patternData = m_processor.m_algo.getPatternData();
        patternData.setValue(patternIndex, stepIndex, dfd::StepParam::Active, 1.0f);
        patternData.setValue(patternIndex, stepIndex, dfd::StepParam::Note, clampedNote);
        patternData.setValue(patternIndex, stepIndex, dfd::StepParam::Velocity, clampedVelocity);
        m_stepGrid.refreshFromData();
        m_processor.commitPatternData();
        showInfo("Pattern " + juce::String(patternIndex + 1) + ", step " + juce::String(stepIndex + 1) + " set by MIDI: note " + juce::String(dfd::noteName(static_cast<int>(clampedNote)))
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
    const bool isPlaying = m_processor.m_algo.isInternalPlay();
    m_processor.m_algo.setInternalPlay(isPlaying == false);
    m_playButton.setToggleState(isPlaying == false, juce::dontSendNotification);
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
    startDropTargetIfNeeded();
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
    refreshPatternViews();
    const bool running = m_processor.m_algo.isTransportRunning();
    const int playingPattern = m_processor.m_algo.getCurrentPattern();
    int framedStep = -1;                        // yellow frame: nothing
    if (running == false)
    {
        framedStep = m_stepGrid.getSelectedStep(); // the transport stands: the selected step
    }
    else if (playingPattern == currentEditPattern())
    {
        framedStep = m_processor.m_algo.getCurrentStep(); // the step that sounds, if its pattern is the shown one
    }
    m_stepGrid.update(framedStep, m_processor.m_algo.getInvalidStepMask(currentEditPattern()),
                      static_cast<int>(std::lround(m_lengthSlider.getValue())));
    m_patternSelector.setPlayingPattern(running ? playingPattern : -1);
    updateNotice();
}

bool DrumFeatureDemonstratorGUI::isInterestedInFileDrag(const juce::StringArray& files)
{
    dfd::debugLog("gui: isInterestedInFileDrag, files: " + files.joinIntoString(" | "));
    return files.size() > 0;
}

void DrumFeatureDemonstratorGUI::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    dfd::debugLog("gui: filesDropped, files: " + files.joinIntoString(" | "));
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
    dfd::debugLog("gui: loadSampleFile " + file.getFullPathName());
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
    for (juce::Label* label : {&m_sampleLabel, &m_baseNoteLabel, &m_lengthLabel, &m_stepValueLabel, &m_tempoLabel, &m_hostBpmLabel, &m_noticeLabel, &m_infoLabel, &m_pageLabel})
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
    placeControlWithLabel(secondRow.removeFromLeft(4 * unit).reduced(margin / 2, 0), m_stepValueLabel, m_stepValueCombo);
    m_noticeLabel.setBounds(secondRow);

    r.removeFromTop(margin);
    m_waveform.setBounds(r.removeFromTop(static_cast<int>(kWaveformFraction * static_cast<float>(height))));
    r.removeFromTop(margin);
    auto patternStrip = r.removeFromTop(static_cast<int>(kPatternStripFraction * static_cast<float>(height)));
    m_patternSelector.setBounds(patternStrip.removeFromLeft(static_cast<int>(kPatternStripWidthFraction * static_cast<float>(r.getWidth()))));
    r.removeFromTop(margin);
    auto bottomBar = r.removeFromBottom(static_cast<int>(kBottomBarFraction * static_cast<float>(height)));
    const int pageUnit = bottomBar.getWidth() / 20;
    m_pageLabel.setBounds(bottomBar.removeFromLeft(2 * pageUnit));
    m_pageNoteButton.setBounds(bottomBar.removeFromLeft(2 * pageUnit).reduced(margin / 2, 1));
    m_pageVelocityButton.setBounds(bottomBar.removeFromLeft(2 * pageUnit).reduced(margin / 2, 1));
    m_pagePanButton.setBounds(bottomBar.removeFromLeft(2 * pageUnit).reduced(margin / 2, 1));
    m_infoLabel.setBounds(bottomBar.reduced(margin, 0));
    m_stepGrid.setBounds(r);
}
