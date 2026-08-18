#pragma once

#include "library/MorphPresetStore.h"
#include <juce_core/juce_core.h>

namespace fmlib
{

void writeMorphVoice (juce::XmlElement& parent, const char* tag, const VoiceData& v, const std::string& name);
bool readMorphVoice (const juce::XmlElement& parent, const char* tag, VoiceData& v, std::string& name);

/** Writes a `<preset>`-shaped element (attributes + a/b/c/d children). */
void writeMorphPresetElement (juce::XmlElement& el, const MorphPreset& p, bool includeName = true);

/** Reads a `<preset>` element; returns false if the tag is wrong. */
bool readMorphPresetElement (const juce::XmlElement& el, MorphPreset& p);

} // namespace fmlib
