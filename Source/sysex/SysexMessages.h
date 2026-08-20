#pragma once

#include "sysex/Dx7Formats.h"
#include "sysex/Tx7Function.h"
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

    /** TX7 1-performance / function edit-buffer bulk. channel 1..16 */
    static std::vector<uint8_t> makePerformanceBulk (const Tx7PerformanceData& data, int channel);

    /** Dump request for 1 voice or bank. channel 1..16 */
    static std::vector<uint8_t> makeDumpRequest (bool bank32, int channel);

    /** Dump request by Yamaha format byte (0x00 voice, 0x01 performance, 0x09 bank). channel 1..16 */
    static std::vector<uint8_t> makeDumpRequestFormat (uint8_t format, int channel);

    /**
     * DX7 voice parameter change (edit buffer). paramIndex 0..154 matches VoiceData indices.
     * channel 1..16.
     */
    static std::vector<uint8_t> makeParameterChange (int paramIndex, uint8_t value, int channel);

    /** DX function parameter change (g=2). channel 1..16. */
    static std::vector<uint8_t> makeDxFunctionParamChange (DxFunctionParam param, uint8_t value, int channel);

    /** TX7 function parameter change (g=4). channel 1..16. */
    static std::vector<uint8_t> makeTxFunctionParamChange (TxFunctionParam param, uint8_t value, int channel);
};

} // namespace fmlib
