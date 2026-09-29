#pragma once

#include "sysex/Dx7Formats.h"
#include <cstdint>

namespace fmlib
{

class VoiceMorpher
{
public:
    /**
     * Four-corner bilinear morph: (0,0)=A (1,0)=B (0,1)=C (1,1)=D.
     * lockGroups: MorphLockGroup bits; locked indices taken from an unlocked morph
     * at (lockRefX, lockRefY) (default corner A).
     * Always writes ABCD-XX:YY into the voice name; live MIDI may freeze that separately.
     */
    static VoiceData morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                             float x, float y, uint32_t lockGroups = 0,
                             float lockRefX = 0.0f, float lockRefY = 0.0f);

    static VoiceNameBytes nameBytesFromVoice (const VoiceData& voice);
    static void applyNameBytes (VoiceData& voice, const VoiceNameBytes& name);

    /**
     * Live morph name policy for the edit-buffer / MIDI dump:
     * - updateName true (manual click / drag-end): keep morph4 name and store it in frozen.
     * - updateName false (drag / Edge LFO / Note morph): restore frozen into voice;
     *   if frozen is empty, seed it once from the current morph name.
     */
    static void applyLiveNamePolicy (VoiceData& voice, VoiceNameBytes& frozen, bool& frozenValid,
                                     bool updateName);
};

} // namespace fmlib
