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

    /**
     * DX7 voice parameter change (edit buffer). paramIndex 0..154 matches VoiceData indices.
     * channel 1..16.
     */
    static std::vector<uint8_t> makeParameterChange (int paramIndex, uint8_t value, int channel);
};

} // namespace fmlib
