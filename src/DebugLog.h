/**
 * @file DebugLog.h
 * @brief optional diagnostic log: only if the file <temp folder>/dfd_debug.flag exists (touch /tmp/dfd_debug.flag),
 *        the lines go to <temp folder>/dfd_debug.log. Used to find out what a host does (e.g. file drops).
 */
#pragma once

#include <juce_core/juce_core.h>

namespace dfd
{
inline void debugLog(const juce::String& text)
{
    const juce::File tempFolder = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (tempFolder.getChildFile("dfd_debug.flag").existsAsFile() == false)
    {
        return;
    }
    const juce::String line = juce::Time::getCurrentTime().formatted("%H:%M:%S") + " " + text + "\n";
    tempFolder.getChildFile("dfd_debug.log").appendText(line);
}
} // namespace dfd
