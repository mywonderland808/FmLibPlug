#pragma once

#include "sysex/Dx7Formats.h"
#include "sysex/MorphLocks.h"
#include <juce_core/juce_core.h>
#include <cstdint>
#include <string>
#include <vector>

namespace fmlib
{

struct MorphPreset
{
    std::string name;
    VoiceData a {}, b {}, c {}, d {};
    std::string nameA, nameB, nameC, nameD;
    float posX = 0.0f;
    float posY = 0.0f;
    uint32_t lockGroups = morphLockNone;
    /** Pad position used as lock reference (unlocked morph at this point). */
    float lockRefX = 0.0f;
    float lockRefY = 0.0f;
};

class MorphPresetStore
{
public:
    const std::vector<MorphPreset>& items() const { return presets; }
    void clear() { presets.clear(); }
    void add (MorphPreset p);
    void removeAt (int index);
    bool replaceAt (int index, MorphPreset p);

    void loadFromFile (const juce::File& file);
    void saveToFile (const juce::File& file) const;

private:
    std::vector<MorphPreset> presets;
};

} // namespace fmlib
