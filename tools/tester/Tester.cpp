/*
    Tester.cpp (AAT2) -- test tool for the plugin, see HowToTestYourPlugin.md
    Author: J. Bitzer @ TGM, Jade Hochschule
    License: MIT

    Two modes, both use the real plugin code (processor and editor):

      Tester render <in.wav> <out.wav> [options]
          plays a wav file through the plugin and writes the result (32-bit float wav)
      Tester snapshot <out.png> [options]
          opens the plugin window offscreen and saves it as an image

    Options:
      --preset <file.xml>   load a preset (as saved by the preset handler)
      --set <id>=<value>    set a parameter to a value in its own unit, e.g. --set ExampleID=1.5
                            (repeatable; "--list" shows all parameter IDs)
      --audio <in.wav>      snapshot: play this file through the plugin first (meters etc.)
      --seconds <s>         snapshot: how long to play/wait before the snapshot (default 1)
      --blocksize <n>       block size (default 512)
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
#include "../ParameterSpec.h"

// createPluginFilter() (declared by JUCE, defined in PluginProcessor.cpp) is the same function
// a DAW calls to create the plugin, so this tool needs no plugin-specific names.

namespace
{
int usage()
{
    std::cout << "Usage:\n"
                 "  Tester render <in.wav> <out.wav> [options]\n"
                 "  Tester snapshot <out.png> [options]\n"
                 "  Tester --list\n"
                 "  Tester --manual <md|tex>\n"
                 "Options: --preset <file.xml>  --set <id>=<value>  --audio <in.wav>\n"
                 "         --seconds <s>  --blocksize <n>\n";
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

// Plays "audio" (or silence, if empty) through the processor in blocks. Optionally pumps
// the message loop, so an open editor can update (meters, displays).
void process(juce::AudioProcessor& processor, const juce::AudioBuffer<float>& audio, int numSamples,
             int blockSize, juce::AudioBuffer<float>* output, bool pumpMessages)
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
    juce::String presetPath, audioPath;
    double seconds = 1.0;
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
        else if (a.startsWith("--"))             { std::cout << "Unknown option " << a << "\n"; return usage(); }
        else                                     positional.add(a);
    }
    const juce::String mode = positional[0];
    if (!(mode == "render" && positional.size() == 3) && !(mode == "snapshot" && positional.size() == 2))
        return usage();

    // preset and parameters
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
    double sampleRate = 48000.0;
    const juce::String inputPath = mode == "render" ? positional[1] : audioPath;
    if (inputPath.isNotEmpty() && !readWav(fileArg(inputPath), audio, sampleRate))
    {
        std::cout << "Could not read " << inputPath << "\n";
        return 1;
    }
    processor->setRateAndBufferSizeDetails(sampleRate, blockSize);
    processor->prepareToPlay(sampleRate, blockSize);

    if (mode == "render")
    {
        juce::AudioBuffer<float> output(processor->getTotalNumOutputChannels(), audio.getNumSamples());
        process(*processor, audio, audio.getNumSamples(), blockSize, &output, false);
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
    process(*processor, audio, (int) (seconds * sampleRate), blockSize, nullptr, true);
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
