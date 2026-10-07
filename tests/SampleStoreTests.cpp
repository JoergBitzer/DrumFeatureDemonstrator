#include <juce_core/juce_core.h>

#include "../src/SampleStore.h"

class SampleStoreTests : public juce::UnitTest
{
public:
    SampleStoreTests() : juce::UnitTest("SampleStore", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        const juce::File workFolder = juce::File::createTempFile("dfd_store_test");
        workFolder.createDirectory();
        const juce::File sourceFolder = workFolder.getChildFile("source");
        sourceFolder.createDirectory();
        const juce::File samplesFolder = dfd::getSamplesFolder(workFolder);

        testCopyAndDedupe(sourceFolder, samplesFolder);
        testEmbedding(workFolder, samplesFolder);
        workFolder.deleteRecursively();
    }

private:
    static juce::File makeFile(const juce::File& folder, const juce::String& name, const juce::String& content)
    {
        const juce::File file = folder.getChildFile(name);
        file.replaceWithText(content);
        return file;
    }

    void testCopyAndDedupe(const juce::File& sourceFolder, const juce::File& samplesFolder)
    {
        beginTest("copy into the samples folder: same content is reused, other content gets a number");
        expect(samplesFolder.isDirectory());
        const juce::File first = makeFile(sourceFolder, "hit.wav", "content one");
        const juce::File copy = dfd::storeSampleCopy(first, samplesFolder);
        expect(copy.getParentDirectory() == samplesFolder);
        expectEquals(copy.getFileName(), juce::String("hit.wav"));

        expect(dfd::storeSampleCopy(first, samplesFolder) == copy);       // same content: reused
        expect(dfd::storeSampleCopy(copy, samplesFolder) == copy);        // already in the folder

        const juce::File other = makeFile(sourceFolder, "hit.wav", "content two"); // same name, other content
        const juce::File second = dfd::storeSampleCopy(other, samplesFolder);
        expectEquals(second.getFileName(), juce::String("hit_2.wav"));
        expectEquals(copy.loadFileAsString(), juce::String("content one"));
    }

    void testEmbedding(const juce::File& workFolder, const juce::File& samplesFolder)
    {
        beginTest("embedding: encode, restore into an empty folder, never overwrite");
        const juce::File source = makeFile(workFolder, "embed.wav", "some audio bytes");
        const juce::String encoded = dfd::encodeFileAsBase64(source);
        expect(encoded.isNotEmpty());

        const juce::File restored = samplesFolder.getChildFile("restored.wav");
        expect(dfd::restoreFileFromBase64(encoded, restored));
        expectEquals(restored.loadFileAsString(), juce::String("some audio bytes"));

        restored.replaceWithText("changed by the user");
        expect(dfd::restoreFileFromBase64(encoded, restored));
        expectEquals(restored.loadFileAsString(), juce::String("changed by the user"));

        expect(dfd::encodeFileAsBase64(workFolder.getChildFile("missing.wav")).isEmpty());
        expect(dfd::restoreFileFromBase64("!!!not base64!!!", samplesFolder.getChildFile("bad.wav")) == false);
    }
};

static SampleStoreTests s_sampleStoreTests;
