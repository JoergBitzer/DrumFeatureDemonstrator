#include <set>
#include <string>

#include <juce_core/juce_core.h>

#include "../src/PatternLibrary.h"

namespace
{
constexpr int kMinLibraryEntries = 30;
constexpr float kLowestNote = 12.0f;
constexpr float kHighestNote = 108.0f;
} // namespace

class PatternLibraryTests : public juce::UnitTest
{
public:
    PatternLibraryTests() : juce::UnitTest("PatternLibrary", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testEveryEntryIsValid();
        testCategories();
        testParsing();
        testInvalidText();
    }

private:
    static float valueOf(const dfd::PatternSteps& steps, int stepIndex, dfd::StepParam param)
    {
        return steps.values[static_cast<std::size_t>(stepIndex * dfd::kNumValuesPerStep + static_cast<int>(param))];
    }

    void testEveryEntryIsValid()
    {
        beginTest("every library pattern can be parsed, has a hit, and stays inside the ranges");
        expect(dfd::numLibraryEntries() >= kMinLibraryEntries);
        std::set<std::string> names;
        for (int entryIndex = 0; entryIndex < dfd::numLibraryEntries(); ++entryIndex)
        {
            const dfd::LibraryEntry& entry = dfd::libraryEntry(entryIndex);
            dfd::PatternSteps steps;
            expect(dfd::makePatternSteps(entry, steps), juce::String("parse: ") + entry.name);
            names.insert(std::string(entry.category) + "/" + entry.name);
            bool anyHit = false;
            for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
            {
                if (valueOf(steps, stepIndex, dfd::StepParam::Active) > 0.5f)
                {
                    anyHit = true;
                }
                expect(valueOf(steps, stepIndex, dfd::StepParam::Note) >= kLowestNote);
                expect(valueOf(steps, stepIndex, dfd::StepParam::Note) <= kHighestNote);
                expect(valueOf(steps, stepIndex, dfd::StepParam::Velocity) >= dfd::kMinVelocity);
                expect(valueOf(steps, stepIndex, dfd::StepParam::Velocity) <= dfd::kMaxVelocity);
                expect(valueOf(steps, stepIndex, dfd::StepParam::Repeat) >= dfd::kMinRepeat);
                expect(valueOf(steps, stepIndex, dfd::StepParam::Repeat) <= dfd::kMaxRepeat);
            }
            expect(anyHit, juce::String("has a hit: ") + entry.name);
        }
        expectEquals(static_cast<int>(names.size()), dfd::numLibraryEntries()); // no name is used twice in a category
    }

    void testCategories()
    {
        beginTest("the categories are BD, SD, HH, Perc and Chromatic, and each has patterns");
        expectEquals(dfd::numLibraryCategories(), 5);
        expectEquals(juce::String(dfd::libraryCategoryName(0)), juce::String("BD"));
        expectEquals(juce::String(dfd::libraryCategoryName(4)), juce::String("Chromatic"));
        for (int categoryIndex = 0; categoryIndex < dfd::numLibraryCategories(); ++categoryIndex)
        {
            int count = 0;
            for (int entryIndex = 0; entryIndex < dfd::numLibraryEntries(); ++entryIndex)
            {
                if (std::string(dfd::libraryEntry(entryIndex).category) == dfd::libraryCategoryName(categoryIndex))
                {
                    ++count;
                }
            }
            expect(count > 0);
        }
    }

    void testParsing()
    {
        beginTest("tokens: hit kinds, note offsets, repeats and ramps");
        const dfd::LibraryEntry entry{"Test", "Tokens", "X . o g x+7 x-5 x*3 x*4^ x*5v . . . . . . ."};
        dfd::PatternSteps steps;
        expect(dfd::makePatternSteps(entry, steps));
        expectEquals(valueOf(steps, 0, dfd::StepParam::Active), 1.0f);
        expectEquals(valueOf(steps, 0, dfd::StepParam::Velocity), 127.0f);
        expectEquals(valueOf(steps, 1, dfd::StepParam::Active), 0.0f);
        expectEquals(valueOf(steps, 2, dfd::StepParam::Velocity), 60.0f);
        expectEquals(valueOf(steps, 3, dfd::StepParam::Velocity), 40.0f);
        expectEquals(valueOf(steps, 4, dfd::StepParam::Note), 67.0f);
        expectEquals(valueOf(steps, 5, dfd::StepParam::Note), 55.0f);
        expectEquals(valueOf(steps, 6, dfd::StepParam::Repeat), 3.0f);
        expectEquals(valueOf(steps, 7, dfd::StepParam::Repeat), 4.0f);
        expect(valueOf(steps, 7, dfd::StepParam::Ramp) > 0.0f);
        expect(valueOf(steps, 8, dfd::StepParam::Ramp) < 0.0f);
        expectEquals(valueOf(steps, 20, dfd::StepParam::Active), 0.0f); // behind the last token
    }

    void testInvalidText()
    {
        beginTest("invalid text is refused and gives an empty pattern");
        dfd::PatternSteps steps;
        expect(dfd::makePatternSteps(dfd::LibraryEntry{"Test", "Bad kind", "X . . . . . . . q . . ."}, steps) == false);
        expect(dfd::makePatternSteps(dfd::LibraryEntry{"Test", "Bad number", "X . . . . . . . x+ . ."}, steps) == false);
        expect(dfd::makePatternSteps(dfd::LibraryEntry{"Test", "Too short", "X . ."}, steps) == false);
        for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
        {
            expectEquals(valueOf(steps, stepIndex, dfd::StepParam::Active), 0.0f);
        }
    }
};

static PatternLibraryTests s_patternLibraryTests;
