#include "SampleLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace dfd
{
namespace
{
constexpr int kMaxChannels = 2; // more channels in the file are ignored
}

std::shared_ptr<const SampleData> loadSampleFile(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        return nullptr;
    }
    if (reader->lengthInSamples <= 0)
    {
        return nullptr;
    }

    const int numChannels = juce::jmin(static_cast<int>(reader->numChannels), kMaxChannels);
    const int numSamples = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, true);

    auto sample = std::make_shared<SampleData>();
    sample->sampleRate = reader->sampleRate;
    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
    {
        const float* source = buffer.getReadPointer(channelIndex);
        sample->channels.emplace_back(source, source + numSamples);
    }
    return sample;
}
} // namespace dfd
