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
     */
    static VoiceData morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                             float x, float y, uint32_t lockGroups = 0,
                             float lockRefX = 0.0f, float lockRefY = 0.0f);
};

} // namespace fmlib
