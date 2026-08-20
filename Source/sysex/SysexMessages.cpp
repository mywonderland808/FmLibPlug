#include "sysex/SysexMessages.h"
#include <cstring>

namespace fmlib
{

namespace
{
uint8_t channelNibble (int channel)
{
    const int c = (channel < 1 ? 1 : (channel > 16 ? 16 : channel)) - 1;
    return static_cast<uint8_t> (c & 0x0f);
}
} // namespace

std::vector<uint8_t> SysexMessages::makeSingleVoiceDump (const VoiceData& voice, int channel)
{
    std::vector<uint8_t> msg;
    msg.reserve (163);
    msg.push_back (0xf0);
    msg.push_back (kYamahaId);
    msg.push_back (static_cast<uint8_t> (0x00 | channelNibble (channel)));
    msg.push_back (kFormatSingleVoice);
    msg.push_back (0x01); // byte count MSB
    msg.push_back (0x1b); // byte count LSB = 155
    msg.insert (msg.end(), voice.begin(), voice.end());
    msg.push_back (yamahaChecksum (voice.data(), voice.size()));
    msg.push_back (0xf7);
    return msg;
}

std::vector<uint8_t> SysexMessages::makeBankDump (const std::array<VoiceData, kBankVoiceCount>& voices, int channel)
{
    std::array<uint8_t, kPackedBankBytes> packed {};
    for (int i = 0; i < kBankVoiceCount; ++i)
    {
        const auto p = packVoice (voices[static_cast<size_t> (i)]);
        std::memcpy (packed.data() + i * kPackedVoiceBytes, p.data(), kPackedVoiceBytes);
    }

    std::vector<uint8_t> msg;
    msg.reserve (4104);
    msg.push_back (0xf0);
    msg.push_back (kYamahaId);
    msg.push_back (static_cast<uint8_t> (0x00 | channelNibble (channel)));
    msg.push_back (kFormatVoiceBank);
    msg.push_back (0x20);
    msg.push_back (0x00);
    msg.insert (msg.end(), packed.begin(), packed.end());
    msg.push_back (yamahaChecksum (packed.data(), packed.size()));
    msg.push_back (0xf7);
    return msg;
}

std::vector<uint8_t> SysexMessages::makePerformanceBulk (const Tx7PerformanceData& data, int channel)
{
    std::vector<uint8_t> msg;
    msg.reserve (102);
    msg.push_back (0xf0);
    msg.push_back (kYamahaId);
    msg.push_back (static_cast<uint8_t> (0x00 | channelNibble (channel)));
    msg.push_back (kFormatPerformance);
    msg.push_back (0x00); // byte count MSB
    msg.push_back (0x5e); // byte count LSB = 94
    msg.insert (msg.end(), data.begin(), data.end());
    msg.push_back (yamahaChecksum (data.data(), data.size()));
    msg.push_back (0xf7);
    return msg;
}

std::vector<uint8_t> SysexMessages::makeDumpRequest (bool bank32, int channel)
{
    return makeDumpRequestFormat (bank32 ? kFormatVoiceBank : kFormatSingleVoice, channel);
}

std::vector<uint8_t> SysexMessages::makeDumpRequestFormat (uint8_t format, int channel)
{
    return {
        0xf0,
        kYamahaId,
        static_cast<uint8_t> (0x20 | channelNibble (channel)),
        static_cast<uint8_t> (format & 0x7f),
        0xf7
    };
}

std::vector<uint8_t> SysexMessages::makeParameterChange (int paramIndex, uint8_t value, int channel)
{
    const int p = (paramIndex < 0 ? 0 : (paramIndex > 154 ? 154 : paramIndex));
    // Voice g=0; hh selects param 0..127 vs 128..155 (Yamaha 0ggggghh).
    const uint8_t groupByte = yamahaParamGroupByte (0, p >= 128 ? 1 : 0);
    return {
        0xf0,
        kYamahaId,
        static_cast<uint8_t> (0x10 | channelNibble (channel)),
        groupByte,
        static_cast<uint8_t> (p & 0x7f),
        static_cast<uint8_t> (value & 0x7f),
        0xf7
    };
}

std::vector<uint8_t> SysexMessages::makeDxFunctionParamChange (DxFunctionParam param, uint8_t value, int channel)
{
    // g=2, h=0 → group byte 0x08
    return {
        0xf0,
        kYamahaId,
        static_cast<uint8_t> (0x10 | channelNibble (channel)),
        yamahaParamGroupByte (2, 0),
        static_cast<uint8_t> (static_cast<uint8_t> (param) & 0x7f),
        static_cast<uint8_t> (value & 0x7f),
        0xf7
    };
}

std::vector<uint8_t> SysexMessages::makeTxFunctionParamChange (TxFunctionParam param, uint8_t value, int channel)
{
    // g=4, h=1 → group byte 0x11 (TX7 MIDI impl. 0ggggghh)
    return {
        0xf0,
        kYamahaId,
        static_cast<uint8_t> (0x10 | channelNibble (channel)),
        yamahaParamGroupByte (4, 1),
        static_cast<uint8_t> (static_cast<uint8_t> (param) & 0x7f),
        static_cast<uint8_t> (value & 0x7f),
        0xf7
    };
}

} // namespace fmlib
