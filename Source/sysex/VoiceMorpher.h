#pragma once

#include "sysex/Dx7Formats.h"

namespace fmlib
{

class VoiceMorpher
{
public:
    /** Two-voice morph (legacy): blend A->B using average of x,y. */
    static VoiceData morph (const VoiceData& a, const VoiceData& b, float x, float y);

    /** Four-corner bilinear morph: (0,0)=A (1,0)=B (0,1)=C (1,1)=D.
        TODO(next): optimize — full 155-byte lerp every drag event is correct but slow over MIDI. */
    static VoiceData morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                             float x, float y);
};

} // namespace fmlib
