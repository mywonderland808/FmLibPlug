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

std::vector<uint8_t> SysexMessages::makeDumpRequest (bool bank32, int channel)
{
    return {
        0xf0,
        kYamahaId,
        static_cast<uint8_t> (0x20 | channelNibble (channel)),
        static_cast<uint8_t> (bank32 ? kFormatVoiceBank : kFormatSingleVoice),
        0xf7
    };
}

} // namespace fmlib
