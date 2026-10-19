/**
 * @file NamedValueSettable.h
 * @brief the small interface of a processor with settings that are not host parameters (used by the Tester without the plugin headers)
 *
 * The Tester sets parameters with --set <id>=<value>; for a name that is no parameter it asks this interface.
 */
#pragma once

#include <juce_core/juce_core.h>

namespace dfd
{
class NamedValueSettable
{
public:
    virtual ~NamedValueSettable() = default;
    // false if the name is unknown
    virtual bool setNamedValue(const juce::String& name, float value) = 0;
    // one line per name: "Name  \"description\"  range  default", for the Tester's --list
    virtual juce::StringArray describeNamedValues() const = 0;
};
} // namespace dfd
