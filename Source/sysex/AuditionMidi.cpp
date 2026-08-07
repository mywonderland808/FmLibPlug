#include "sysex/AuditionMidi.h"
#include <algorithm>

namespace fmlib
{

namespace
{
int clamp7 (int v)
{
    return std::max (0, std::min (127, v));
}
int clampCh (int c)
{
    return std::max (1, std::min (16, c));
}
} // namespace

std::vector<uint8_t> AuditionMidi::makeNoteOn (int channel, int note, int velocity)
{
    const int ch = clampCh (channel) - 1;
    return { static_cast<uint8_t> (0x90 | ch), static_cast<uint8_t> (clamp7 (note)), static_cast<uint8_t> (clamp7 (velocity)) };
}

std::vector<uint8_t> AuditionMidi::makeNoteOff (int channel, int note)
{
    const int ch = clampCh (channel) - 1;
    return { static_cast<uint8_t> (0x80 | ch), static_cast<uint8_t> (clamp7 (note)), 0 };
}

} // namespace fmlib
