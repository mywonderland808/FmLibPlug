#pragma once

#include <cstdint>
#include <vector>

namespace fmlib
{

class AuditionMidi
{
public:
    /** channel 1..16, note/velocity 0..127 */
    static std::vector<uint8_t> makeNoteOn (int channel, int note, int velocity);
    static std::vector<uint8_t> makeNoteOff (int channel, int note);
};

} // namespace fmlib
