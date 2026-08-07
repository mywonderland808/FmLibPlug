#pragma once

#include "sysex/Dx7Formats.h"
#include <array>
#include <vector>

namespace fmlib
{

class SysexMessages
{
public:
    /** Build 1-voice bulk dump to edit buffer. channel 1..16 */
    static std::vector<uint8_t> makeSingleVoiceDump (const VoiceData& voice, int channel);

    /** Build 32-voice bank dump. channel 1..16 */
    static std::vector<uint8_t> makeBankDump (const std::array<VoiceData, kBankVoiceCount>& voices, int channel);

    /** Dump request for 1 voice or bank. channel 1..16 */
    static std::vector<uint8_t> makeDumpRequest (bool bank32, int channel);
};

} // namespace fmlib
