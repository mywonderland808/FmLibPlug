#pragma once

#include <juce_core/juce_core.h>

namespace fmlib
{

inline constexpr const char* kDawMidiPortName = "(DAW)";

inline bool isDawMidiPortName (const juce::String& name)
{
    return name == kDawMidiPortName;
}

} // namespace fmlib
