/*
    Tester.cpp (AAT2) -- test tool for the plugin, see HowToTestYourPlugin.md
    Author: J. Bitzer @ TGM, Jade Hochschule
    License: MIT

    Two modes, both use the real plugin code (processor and editor):

      Tester render <in.wav|-> <out.wav> [options]
          plays a wav file through the plugin and writes the result (32-bit float wav);
          "-" = no input (instrument): the length is --seconds, the rate --rate
      Tester snapshot <out.png> [options]
          opens the plugin window offscreen and saves it as an image

    Options:
      --preset <file.xml>   load a preset (as saved by the preset handler)
      --set <id>=<value>    set a parameter to a value in its own unit, e.g. --set ExampleID=1.5
                            (repeatable; "--list" shows all parameter IDs)
      --audio <in.wav>      snapshot: play this file through the plugin first (meters etc.)
      --seconds <s>         snapshot: how long to play/wait before the snapshot (default 1)
      --blocksize <n>       block size (default 512)
      --rate <hz>           sample rate if there is no input file (default 48000)
      --sample <file>       instrument: load this sample (wav, aiff, flac, ogg); repeatable: each one replaces the one before
      --note <n>[,<vel>[,<seconds>]]   instrument: MIDI note n (velocity 1-127, default 100) at the time
                            given in seconds (default 0); repeatable
      --save-state <file>   write the plugin state (as a DAW saves it) to a file, after everything else was set
      --load-state <file>   restore such a state first (before --preset, --set, --sample)
      --click <x,y>         snapshot: a mouse click (position as fractions of the window size, e.g. 0.5,0.9); repeatable, in the
                            order given, after --keys (synthetic events for the window of the Tester, nothing of the desktop)
      --drag <x1,y1,x2,y2>  snapshot: press the mouse at (x1,y1), drag to (x2,y2) in 25 steps and release (fractions of the window size);
                            repeatable, after --click
      --keys <list>         snapshot: send keys to the window after the notes: left,right,up,down,home,end,space (comma separated); afterwards 0.5 s are played
      --drop <file>         snapshot: simulate dropping this file onto the window (at the given --drop-at position) before the snapshot
      --drop-at <x,y>       position of the simulated drop as fractions of the window size (default 0.5,0.3)
      --list                print all parameters (ID, name, range, default) and exit
      --manual <md|tex>     print the list of controls as a Markdown or LaTeX table and exit
                            (range and default as the plugin shows them, description = the
                            parameter's "help" line, see tools/ParameterSpec.h)

    Note: like in a DAW, the plugin uses your preset folder and settings. On Linux/macOS, run
    it with a temporary home folder to keep them untouched: HOME=$(mktemp -d) Tester ...
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <memory>
#include <vector>
#include "../ParameterSpec.h"
#include "../../src/SampleLoadable.h" // --sample: any processor that can load a sample

// createPluginFilter() (declared by JUCE, defined in PluginProcessor.cpp) is the same function
// a DAW calls to create the plugin, so this tool needs no plugin-specific names.

namespace
{
int usage()
{
    std::cout << "Usage:\n"
                 "  Tester render <in.wav|-> <out.wav> [options]\n"
                 "  Tester snapshot <out.png> [options]\n"
                 "  Tester --list\n"
                 "  Tester --manual <md|tex>\n"
                 "Options: --preset <file.xml>  --set <id>=<value>  --audio <in.wav>\n"
                 "         --seconds <s>  --blocksize <n>  --rate <hz>\n"
                 "         --sample <file>  --note <n>[,<vel>[,<seconds>]]\n";
    return 2;
}

juce::File fileArg(const juce::String& path)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile(path);
}

bool loadPreset(juce::AudioProcessor& processor, const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr)
        return false;
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    processor.setStateInformation(block.getData(), (int) block.getSize());
    return true;
}

juce::RangedAudioParameter* findParameter(juce::AudioProcessor& processor, const juce::String& id)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (ranged->getParameterID() == id)
                return ranged;
    return nullptr;
}

void listParameters(juce::AudioProcessor& processor)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            const auto& range = ranged->getNormalisableRange();
            std::cout << ranged->getParameterID() << "  \"" << ranged->getName(100) << "\"  "
                      << range.start << " .. " << range.end << "  default "
                      << range.convertFrom0to1(ranged->getDefaultValue()) << "\n";
        }
}

juce::String texEscape(const juce::String& t)
{
    juce::String out;
    for (auto c : t)
    {
        if (c == '\\')                           out << "\\textbackslash{}";
        else if (juce::String("&%$#_{}").containsChar(c)) out << "\\" << juce::String::charToString(c);
        else                                     out << juce::String::charToString(c);
    }
    return out;
}

// the list of controls for a manual: Markdown or LaTeX table
void printManual(juce::AudioProcessor& processor, const juce::String& format)
{
    const bool tex = format == "tex";
    if (tex)
        std::cout << "\\begin{tabular}{p{0.2\\textwidth}p{0.22\\textwidth}p{0.12\\textwidth}p{0.36\\textwidth}}\n"
                     "\\hline\nControl & Range & Default & Description \\\\\n\\hline\n";
    else
        std::cout << "| Control | Range | Default | Description |\n|---|---|---|---|\n";
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            const auto help = jade::parameterHelpTexts().count(ranged->getParameterID()) > 0
                                  ? jade::parameterHelpTexts()[ranged->getParameterID()] : juce::String();
            const juce::String cells[] = { ranged->getName(100),
                                           ranged->getText(0.0f, 100) + " - " + ranged->getText(1.0f, 100),
                                           ranged->getText(ranged->getDefaultValue(), 100), help };
            if (tex)
                std::cout << texEscape(cells[0]) << " & " << texEscape(cells[1]) << " & " << texEscape(cells[2])
                          << " & " << texEscape(cells[3]) << " \\\\\n";
            else
                std::cout << "| " << cells[0] << " | " << cells[1] << " | " << cells[2] << " | " << cells[3] << " |\n";
        }
    if (tex)
        std::cout << "\\hline\n\\end{tabular}\n";
}

struct NoteEvent
{
    int noteNumber = 0;
    int velocity = 0;
    int samplePosition = 0;
};

// Plays "audio" (or silence, if empty) through the processor in blocks. Optionally pumps
// the message loop, so an open editor can update (meters, displays).
void process(juce::AudioProcessor& processor, const juce::AudioBuffer<float>& audio, int numSamples,
             int blockSize, juce::AudioBuffer<float>* output, bool pumpMessages,
             const std::vector<NoteEvent>& notes = {})
{
    const int numChannels = juce::jmax(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> block(numChannels, blockSize);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        const int n = juce::jmin(blockSize, numSamples - pos);
        block.setSize(numChannels, n, false, false, true);
        block.clear();
        for (int ch = 0; ch < processor.getTotalNumInputChannels() && audio.getNumChannels() > 0; ++ch)
            block.copyFrom(ch, 0, audio, juce::jmin(ch, audio.getNumChannels() - 1), pos % audio.getNumSamples(),
                           juce::jmin(n, audio.getNumSamples() - pos % audio.getNumSamples()));
        midi.clear();
        for (const auto& note : notes)
            if (note.samplePosition >= pos && note.samplePosition < pos + n)
                midi.addEvent(juce::MidiMessage::noteOn(1, note.noteNumber, (juce::uint8) note.velocity),
                              note.samplePosition - pos);
        processor.processBlock(block, midi);
        if (output != nullptr)
            for (int ch = 0; ch < output->getNumChannels(); ++ch)
                output->copyFrom(ch, pos, block, juce::jmin(ch, numChannels - 1), 0, n);
        if (pumpMessages)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(juce::jmax(1, 1000 * n / 48000));
    }
}

bool readWav(const juce::File& file, juce::AudioBuffer<float>& audio, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return false;
    audio.setSize((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read(&audio, 0, audio.getNumSamples(), 0, true, true);
    sampleRate = reader->sampleRate;
    return true;
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate(sampleRate)
                             .withNumChannels(audio.getNumChannels())
                             .withBitsPerSample(32)
                             .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = juce::WavAudioFormat().createWriterFor(stream, options);
    return writer != nullptr && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}
} // namespace

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add(juce::String::fromUTF8(argv[i]));
    if (args.isEmpty())
        return usage();

    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());

    if (args.contains("--list"))
    {
        listParameters(*processor);
        return 0;
    }
    if (const int i = args.indexOf("--manual"); i >= 0)
    {
        if (args[i + 1] != "md" && args[i + 1] != "tex")
            return usage();
        printManual(*processor, args[i + 1]);
        return 0;
    }

    // options
    juce::String presetPath, audioPath, saveStatePath, loadStatePath, dropPath, dropAt = "0.5,0.3", keyNames;
    juce::StringArray clickArgs, dragArgs;
    juce::StringArray samplePaths;
    juce::StringArray noteArgs;
    double seconds = 1.0;
    double rate = 48000.0;
    int blockSize = 512;
    juce::StringArray settings;
    juce::StringArray positional;
    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        const bool hasValue = i + 1 < args.size();
        if (a == "--preset" && hasValue)         presetPath = args[++i];
        else if (a == "--set" && hasValue)       settings.add(args[++i]);
        else if (a == "--audio" && hasValue)     audioPath = args[++i];
        else if (a == "--seconds" && hasValue)   seconds = args[++i].getDoubleValue();
        else if (a == "--blocksize" && hasValue) blockSize = juce::jmax(16, args[++i].getIntValue());
        else if (a == "--save-state" && hasValue) saveStatePath = args[++i];
        else if (a == "--load-state" && hasValue) loadStatePath = args[++i];
        else if (a == "--drag" && hasValue)      dragArgs.add(args[++i]);
        else if (a == "--click" && hasValue)     clickArgs.add(args[++i]);
        else if (a == "--keys" && hasValue)      keyNames = args[++i];
        else if (a == "--drop" && hasValue)      dropPath = args[++i];
        else if (a == "--drop-at" && hasValue)   dropAt = args[++i];
        else if (a == "--rate" && hasValue)      rate = args[++i].getDoubleValue();
        else if (a == "--sample" && hasValue)    samplePaths.add(args[++i]);
        else if (a == "--note" && hasValue)      noteArgs.add(args[++i]);
        else if (a.startsWith("--"))             { std::cout << "Unknown option " << a << "\n"; return usage(); }
        else                                     positional.add(a);
    }
    const juce::String mode = positional[0];
    if (!(mode == "render" && positional.size() == 3) && !(mode == "snapshot" && positional.size() == 2))
        return usage();

    // state, preset and parameters
    if (loadStatePath.isNotEmpty())
    {
        juce::MemoryBlock stateData;
        if (!fileArg(loadStatePath).loadFileAsData(stateData))
        {
            std::cout << "Could not read state " << loadStatePath << "\n";
            return 1;
        }
        processor->setStateInformation(stateData.getData(), (int) stateData.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300); // a plugin may finish loading asynchronously
    }
    if (presetPath.isNotEmpty() && !loadPreset(*processor, fileArg(presetPath)))
    {
        std::cout << "Could not read preset " << presetPath << "\n";
        return 1;
    }
    for (const auto& s : settings)
    {
        auto* p = findParameter(*processor, s.upToFirstOccurrenceOf("=", false, false));
        if (p == nullptr || !s.contains("="))
        {
            std::cout << "Unknown parameter in --set " << s << " (see --list)\n";
            return 1;
        }
        p->setValueNotifyingHost(p->convertTo0to1(s.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
    }

    // audio
    juce::AudioBuffer<float> audio;
    double sampleRate = rate;
    juce::String inputPath = mode == "render" ? positional[1] : audioPath;
    if (inputPath == "-")
        inputPath = juce::String();
    if (inputPath.isNotEmpty() && !readWav(fileArg(inputPath), audio, sampleRate))
    {
        std::cout << "Could not read " << inputPath << "\n";
        return 1;
    }
    processor->setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor->prepareToPlay(sampleRate, blockSize);

    // instrument: sample and notes
    for (const auto& samplePath : samplePaths) // several: loaded one after the other, the last one stays
    {
        auto* drumPlugin = dynamic_cast<dfd::SampleLoadable*>(processor.get());
        if (drumPlugin == nullptr || !drumPlugin->loadSample(fileArg(samplePath)))
        {
            std::cout << "Could not load sample " << samplePath << "\n";
            return 1;
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    if (saveStatePath.isNotEmpty())
    {
        juce::MemoryBlock stateData;
        processor->getStateInformation(stateData);
        if (!fileArg(saveStatePath).replaceWithData(stateData.getData(), stateData.getSize()))
        {
            std::cout << "Could not write state " << saveStatePath << "\n";
            return 1;
        }
        std::cout << "wrote state " << saveStatePath << " (" << stateData.getSize() << " bytes)\n";
    }
    std::vector<NoteEvent> notes;
    for (const auto& n : noteArgs)
    {
        juce::StringArray parts = juce::StringArray::fromTokens(n, ",", "");
        NoteEvent note;
        note.noteNumber = parts[0].getIntValue();
        note.velocity = parts.size() > 1 ? parts[1].getIntValue() : 100;
        note.samplePosition = parts.size() > 2 ? (int) std::lround(parts[2].getDoubleValue() * sampleRate) : 0;
        notes.push_back(note);
    }

    if (mode == "render")
    {
        const int numSamples = audio.getNumSamples() > 0 ? audio.getNumSamples() : (int) (seconds * sampleRate);
        juce::AudioBuffer<float> output(processor->getTotalNumOutputChannels(), numSamples);
        process(*processor, audio, numSamples, blockSize, &output, false, notes);
        processor->releaseResources();
        if (!writeWav(fileArg(positional[2]), output, sampleRate))
        {
            std::cout << "Could not write " << positional[2] << "\n";
            return 1;
        }
        std::cout << "wrote " << positional[2] << " (" << output.getNumChannels() << " channels, "
                  << output.getNumSamples() << " samples)\n";
        return 0;
    }

    // snapshot
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorAndMakeActive());
    if (editor == nullptr)
    {
        std::cout << "The plugin has no editor\n";
        return 1;
    }
    editor->setVisible(true);
    editor->addToDesktop(0); // offscreen window: some components only paint with a peer
    process(*processor, audio, (int) (seconds * sampleRate), blockSize, nullptr, true, notes);
    if (keyNames.isNotEmpty())
    {
        auto* peer = editor->getPeer();
        const juce::StringArray names = juce::StringArray::fromTokens(keyNames, ",", "");
        for (const auto& name : names)
        {
            int keyCode = 0;
            if (name == "left")  keyCode = juce::KeyPress::leftKey;
            if (name == "right") keyCode = juce::KeyPress::rightKey;
            if (name == "up")    keyCode = juce::KeyPress::upKey;
            if (name == "down")  keyCode = juce::KeyPress::downKey;
            if (name == "home")  keyCode = juce::KeyPress::homeKey;
            if (name == "end")   keyCode = juce::KeyPress::endKey;
            if (name == "space") keyCode = juce::KeyPress::spaceKey;
            if (peer == nullptr || keyCode == 0)
            {
                std::cout << "Cannot send key " << name << "\n";
                return 1;
            }
            peer->handleKeyPress(juce::KeyPress(keyCode));
        }
        process(*processor, audio, (int) (0.5 * sampleRate), blockSize, nullptr, true, {});
    }
    for (const auto& clickArg : clickArgs)
    {
        auto* peer = editor->getPeer();
        const auto fractions = juce::StringArray::fromTokens(clickArg, ",", "");
        if (peer == nullptr || fractions.size() < 2)
        {
            std::cout << "Cannot send the click " << clickArg << "\n";
            return 1;
        }
        const juce::Point<float> position((float) (fractions[0].getDoubleValue() * editor->getWidth()),
                                          (float) (fractions[1].getDoubleValue() * editor->getHeight()));
        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        const juce::int64 now = juce::Time::currentTimeMillis();
        const auto noPressure = juce::MouseInputSource::defaultPressure;
        const auto noOrientation = juce::MouseInputSource::defaultOrientation;
        peer->handleMouseEvent(source.getType(), position, juce::ModifierKeys(), noPressure, noOrientation, now);
        peer->handleMouseEvent(source.getType(), position, juce::ModifierKeys::leftButtonModifier, noPressure, noOrientation, now);
        peer->handleMouseEvent(source.getType(), position, juce::ModifierKeys(), noPressure, noOrientation, now);
        process(*processor, audio, (int) (0.1 * sampleRate), blockSize, nullptr, true, {});
    }
    for (const auto& dragArg : dragArgs)
    {
        auto* peer = editor->getPeer();
        const auto fractions = juce::StringArray::fromTokens(dragArg, ",", "");
        if (peer == nullptr || fractions.size() < 4)
        {
            std::cout << "Cannot send the drag " << dragArg << "\n";
            return 1;
        }
        auto positionOf = [&](double fx, double fy)
        { return juce::Point<float>((float) (fx * editor->getWidth()), (float) (fy * editor->getHeight())); };
        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto noPressure = juce::MouseInputSource::defaultPressure;
        const auto noOrientation = juce::MouseInputSource::defaultOrientation;
        const auto leftDown = juce::ModifierKeys::leftButtonModifier;
        const juce::int64 now = juce::Time::currentTimeMillis();
        const auto start = positionOf(fractions[0].getDoubleValue(), fractions[1].getDoubleValue());
        const auto end = positionOf(fractions[2].getDoubleValue(), fractions[3].getDoubleValue());
        constexpr int numSteps = 25;
        peer->handleMouseEvent(source.getType(), start, juce::ModifierKeys(), noPressure, noOrientation, now);
        peer->handleMouseEvent(source.getType(), start, leftDown, noPressure, noOrientation, now);
        for (int step = 1; step <= numSteps; ++step)
        {
            const auto position = start + (end - start) * ((float) step / (float) numSteps);
            peer->handleMouseEvent(source.getType(), position, leftDown, noPressure, noOrientation, now);
            process(*processor, audio, (int) (0.01 * sampleRate), blockSize, nullptr, true, {});
        }
        peer->handleMouseEvent(source.getType(), end, juce::ModifierKeys(), noPressure, noOrientation, now);
        process(*processor, audio, (int) (0.1 * sampleRate), blockSize, nullptr, true, {});
    }
    if (dropPath.isNotEmpty())
    {
        auto* peer = editor->getPeer();
        if (peer == nullptr)
        {
            std::cout << "The editor has no window peer, cannot simulate a drop\n";
            return 1;
        }
        juce::ComponentPeer::DragInfo info;
        info.files.add(fileArg(dropPath).getFullPathName());
        const auto fractions = juce::StringArray::fromTokens(dropAt, ",", "");
        info.position = { (int) (fractions[0].getDoubleValue() * editor->getWidth()),
                          (int) (fractions[1].getDoubleValue() * editor->getHeight()) };
        const bool acceptedMove = peer->handleDragMove(info);
        const bool acceptedDrop = peer->handleDragDrop(info);
        std::cout << "simulated drop of " << dropPath << ": move accepted " << acceptedMove << ", drop accepted " << acceptedDrop << "\n";
        process(*processor, audio, (int) (0.3 * sampleRate), blockSize, nullptr, true, {}); // let the GUI and the async loading settle
        if (auto* loadable = dynamic_cast<dfd::SampleLoadable*>(processor.get()))
            std::cout << "sample name now: " << loadable->getSampleName() << "\n";
    }
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    processor->editorBeingDeleted(editor.get()); // as a DAW does, before deleting the editor
    editor.reset();
    processor->releaseResources();

    const auto out = fileArg(positional[1]);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream))
    {
        std::cout << "Could not write " << positional[1] << "\n";
        return 1;
    }
    std::cout << "wrote " << positional[1] << " (" << image.getWidth() << " x " << image.getHeight() << ")\n";
    return 0;
}
