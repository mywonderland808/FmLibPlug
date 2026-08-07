#pragma once

#include "sysex/Dx7Formats.h"

namespace fmlib
{

class VoiceMorpher
{
public:
    /** Four-corner bilinear morph: (0,0)=A (1,0)=B (0,1)=C (1,1)=D. */
    static VoiceData morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                             float x, float y);
};

} // namespace fmlib
