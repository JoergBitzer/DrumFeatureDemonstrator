#include "SampleStore.h"

namespace dfd
{
namespace
{
const char* const kSamplesFolderName = "Samples";
constexpr int kMaxCopies = 1000;

bool haveSameContent(const juce::File& first, const juce::File& second)
{
    if (first.getSize() != second.getSize())
    {
        return false;
    }
    return first.hasIdenticalContentTo(second);
}
} // namespace

juce::File getSamplesFolder(const juce::File& userPresetsFolder)
{
    const juce::File folder = userPresetsFolder.getChildFile(kSamplesFolderName);
    folder.createDirectory();
    return folder;
}

juce::File storeSampleCopy(const juce::File& sourceFile, const juce::File& samplesFolder)
{
    if (sourceFile.getParentDirectory() == samplesFolder)
    {
        return sourceFile;
    }
    const juce::String baseName = sourceFile.getFileNameWithoutExtension();
    const juce::String extension = sourceFile.getFileExtension();
    juce::File target = samplesFolder.getChildFile(baseName + extension);
    for (int copyNumber = 2; copyNumber <= kMaxCopies; ++copyNumber)
    {
        if (target.existsAsFile() == false)
        {
            sourceFile.copyFileTo(target);
            return target;
        }
        if (haveSameContent(sourceFile, target))
        {
            return target;
        }
        target = samplesFolder.getChildFile(baseName + "_" + juce::String(copyNumber) + extension);
    }
    return sourceFile;
}

juce::String encodeFileAsBase64(const juce::File& file)
{
    if (file.existsAsFile() == false || file.getSize() > kMaxEmbeddedSampleBytes)
    {
        return juce::String();
    }
    juce::MemoryBlock content;
    if (file.loadFileAsData(content) == false)
    {
        return juce::String();
    }
    return juce::Base64::toBase64(content.getData(), content.getSize());
}

bool restoreFileFromBase64(const juce::String& base64Text, const juce::File& file)
{
    if (file.existsAsFile())
    {
        return true;
    }
    juce::MemoryOutputStream decoded;
    if (juce::Base64::convertFromBase64(decoded, base64Text) == false)
    {
        return false;
    }
    return file.replaceWithData(decoded.getData(), decoded.getDataSize());
}
} // namespace dfd
