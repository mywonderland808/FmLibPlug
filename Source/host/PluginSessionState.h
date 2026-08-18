#pragma once

#include "library/MorphPresetStore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace fmlib
{

inline constexpr int kPluginSessionStateVersion = 1;

struct PluginSessionState
{
    int stateVersion = kPluginSessionStateVersion;
    juce::ValueTree apvtsState;
    MorphPreset liveMorph;
};

/** Root element name: FmLibPlugState */
std::unique_ptr<juce::XmlElement> writePluginSessionXml (const PluginSessionState& state);
bool readPluginSessionXml (const juce::XmlElement& root, PluginSessionState& state);

} // namespace fmlib
