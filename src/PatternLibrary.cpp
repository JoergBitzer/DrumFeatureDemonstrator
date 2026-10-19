#include "PatternLibrary.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace dfd
{
namespace
{
const std::array<const char*, 5> kCategories = {"BD", "SD", "HH", "Perc", "Chromatic"};

constexpr float kVelocityNormal = 100.0f;
constexpr float kVelocityAccent = 127.0f;
constexpr float kVelocitySoft = 60.0f;
constexpr float kVelocityGhost = 40.0f;
constexpr float kRampFaster = 0.6f;
constexpr float kRampSlower = -0.6f;

// Kick: the base note is the pitch of the sample; offsets only in the chromatic patterns.
const std::array<LibraryEntry, 40> kEntries = {{
    {"BD", "Four on the floor", "X . . . X . . . X . . . X . . ."},
    {"BD", "Disco push", "X . . . X . . . X . . . X . x ."},
    {"BD", "Boom bap", "X . . . . . x . . . x . . . . ."},
    {"BD", "Half time", "X . . . . . x . . . . . . . . ."},
    {"BD", "Funk syncopation", "X . . . . x . . . . X . . x . ."},
    {"BD", "Breakbeat", "X . x . . . . . . . x x . . . ."},
    {"BD", "Trap 808", "X . . . . . . . . . X . . . . x"},
    {"BD", "Double bass 16ths", "X x X x X x X x X x X x X x X x"},

    {"SD", "Backbeat 2 and 4", ". . . . X . . . . . . . X . . ."},
    {"SD", "Half time (on 3)", ". . . . . . . . X . . . . . . ."},
    {"SD", "Boom bap with ghosts", ". . . . X . . g . g . . X . . g"},
    {"SD", "Funk ghost notes", ". . g . X . g . . g . . X . g g"},
    {"SD", "Dembow", ". . . x . . x . . . . x . . x ."},
    {"SD", "Clap flam 2 and 4", ". . . . X*2^ . . . . . . . X*2^ . . ."},
    {"SD", "Rim offbeats", ". . . . . . x . . . . . . . x ."},
    {"SD", "Build up roll", ". . . . . . . . x . x . x*2^ x*3^ x*4^ X*8^"},

    {"HH", "Closed 8ths", "x . x . x . x . x . x . x . x ."},
    {"HH", "Closed 16ths", "x x x x x x x x x x x x x x x x"},
    {"HH", "Accented 16ths", "X x o x X x o x X x o x X x o x"},
    {"HH", "Offbeat open hat", ". . x . . . x . . . x . . . x ."},
    {"HH", "Ands and as", ". x x . . x x . . x x . . x x ."},
    {"HH", "Trap ratchets", "x . x . x . x*3 . x . x . x . x*2^ x*4^"},
    {"HH", "Roll up (speeding up)", ". . . . . . . . . . . . x*8^ . . ."},
    {"HH", "Roll down (slowing down)", "x*8v . . . . . . . . . . . . . . ."},

    {"Perc", "Son clave 3-2", "x . . x . . x . . . x . x . . ."},
    {"Perc", "Rumba clave 3-2", "x . . x . . . x . . x . x . . ."},
    {"Perc", "Bossa nova clave", "x . . x . . x . . . x . . x . ."},
    {"Perc", "Tresillo 3+3+2", "x . . x . . x ."},
    {"Perc", "Cascara", "x . x . x x . x . x . x x . x ."},
    {"Perc", "Shaker 16ths", "X o x o X o x o X o x o X o x o"},
    {"Perc", "Cowbell 8ths", "x . . . x . x . x . . . x . x ."},
    {"Perc", "Conga tumbao", ". . . x . . x x . . . x . . x x"},

    {"Chromatic", "Minor arpeggio", "x x+3 x+7 x+12 x+7 x+3 x x+3"},
    {"Chromatic", "Major arpeggio", "x x+4 x+7 x+12 x+7 x+4 x x+4"},
    {"Chromatic", "Octave bounce", "x x+12 x x+12 x x+12 x x+12"},
    {"Chromatic", "Natural minor run up", "x x+2 x+3 x+5 x+7 x+8 x+10 x+12"},
    {"Chromatic", "Chromatic run up", "x x+1 x+2 x+3 x+4 x+5 x+6 x+7"},
    {"Chromatic", "Walking bass root and fifth", "X . x+7 . x+10 . x+7 . X . x+7 . x+12 . x+10 ."},
    {"Chromatic", "Pentatonic riff", "x . x+3 . x+5 x+7 . x+10 . x+7 . x+5 . x+3 . ."},
    {"Chromatic", "Pedal point", "x x+12 x x+14 x x+15 x x+19"},
}};

bool isHitKind(char kind, float& velocity)
{
    switch (kind)
    {
    case 'x':
        velocity = kVelocityNormal;
        return true;
    case 'X':
        velocity = kVelocityAccent;
        return true;
    case 'o':
        velocity = kVelocitySoft;
        return true;
    case 'g':
        velocity = kVelocityGhost;
        return true;
    default:
        return false;
    }
}

void setStepValue(PatternSteps& steps, int stepIndex, StepParam param, float value)
{
    steps.values[static_cast<std::size_t>(stepIndex * kNumValuesPerStep + static_cast<int>(param))] = value;
}

// parses the modifiers behind a hit (+n, -n, *n, ^, v) into the step; false if something is not understood
bool parseModifiers(const std::string& token, int stepIndex, PatternSteps& steps)
{
    std::size_t position = 1;
    while (position < token.size())
    {
        const char sign = token[position];
        if (sign == '^')
        {
            setStepValue(steps, stepIndex, StepParam::Ramp, kRampFaster);
            ++position;
            continue;
        }
        if (sign == 'v')
        {
            setStepValue(steps, stepIndex, StepParam::Ramp, kRampSlower);
            ++position;
            continue;
        }
        if (sign != '+' && sign != '-' && sign != '*')
        {
            return false;
        }
        std::size_t digitsEnd = position + 1;
        while (digitsEnd < token.size() && std::isdigit(static_cast<unsigned char>(token[digitsEnd])) != 0)
        {
            ++digitsEnd;
        }
        if (digitsEnd == position + 1)
        {
            return false;
        }
        const int number = std::atoi(token.substr(position + 1, digitsEnd - position - 1).c_str());
        if (sign == '*')
        {
            setStepValue(steps, stepIndex, StepParam::Repeat, static_cast<float>(number));
        }
        else
        {
            float offset = static_cast<float>(number);
            if (sign == '-')
            {
                offset = -offset;
            }
            setStepValue(steps, stepIndex, StepParam::Note, kDefaultStepNote + offset);
        }
        position = digitsEnd;
    }
    return true;
}

void setAllOff(PatternSteps& steps)
{
    for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
    {
        for (int paramIndex = 0; paramIndex < kNumValuesPerStep; ++paramIndex)
        {
            const StepParam param = static_cast<StepParam>(paramIndex);
            setStepValue(steps, stepIndex, param, stepValueSpec(param).defaultValue);
        }
    }
}
} // namespace

int numLibraryCategories()
{
    return static_cast<int>(kCategories.size());
}

const char* libraryCategoryName(int categoryIndex)
{
    return kCategories[static_cast<std::size_t>(categoryIndex)];
}

int numLibraryEntries()
{
    return static_cast<int>(kEntries.size());
}

const LibraryEntry& libraryEntry(int entryIndex)
{
    return kEntries[static_cast<std::size_t>(entryIndex)];
}

bool makePatternSteps(const LibraryEntry& entry, PatternSteps& steps)
{
    setAllOff(steps);
    const std::string text(entry.steps);
    int stepIndex = 0;
    std::size_t position = 0;
    while (position < text.size())
    {
        while (position < text.size() && text[position] == ' ')
        {
            ++position;
        }
        if (position >= text.size())
        {
            break;
        }
        std::size_t tokenEnd = text.find(' ', position);
        if (tokenEnd == std::string::npos)
        {
            tokenEnd = text.size();
        }
        const std::string token = text.substr(position, tokenEnd - position);
        position = tokenEnd;
        if (stepIndex >= kMaxSteps)
        {
            setAllOff(steps);
            return false;
        }
        if (token != ".")
        {
            float velocity = 0.0f;
            if (isHitKind(token[0], velocity) == false)
            {
                setAllOff(steps);
                return false;
            }
            setStepValue(steps, stepIndex, StepParam::Active, 1.0f);
            setStepValue(steps, stepIndex, StepParam::Velocity, velocity);
            if (parseModifiers(token, stepIndex, steps) == false)
            {
                setAllOff(steps);
                return false;
            }
        }
        ++stepIndex;
    }
    if (stepIndex < kMinSteps)
    {
        setAllOff(steps);
        return false;
    }
    return true;
}
} // namespace dfd
