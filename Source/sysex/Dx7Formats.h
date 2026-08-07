#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fmlib
{

constexpr int kVoiceDataBytes = 155;
constexpr int kPackedVoiceBytes = 128;
constexpr int kBankVoiceCount = 32;
constexpr int kPackedBankBytes = kBankVoiceCount * kPackedVoiceBytes; // 4096
constexpr int kNameLength = 10;
constexpr uint8_t kYamahaId = 0x43;
constexpr uint8_t kFormatSingleVoice = 0x00;
constexpr uint8_t kFormatVoiceBank = 0x09;

using VoiceData = std::array<uint8_t, kVoiceDataBytes>;
using PackedVoice = std::array<uint8_t, kPackedVoiceBytes>;

inline uint8_t yamahaChecksum (const uint8_t* data, size_t size)
{
    int sum = 0;
    for (size_t i = 0; i < size; ++i)
        sum += data[i] & 0x7f;
    return static_cast<uint8_t> (((~sum) + 1) & 0x7f);
}

inline bool yamahaChecksumOk (const uint8_t* data, size_t size, uint8_t checksum)
{
    return yamahaChecksum (data, size) == (checksum & 0x7f);
}

std::string voiceNameFromData (const VoiceData& voice);
uint64_t contentIdFromVoice (const VoiceData& voice);
VoiceData unpackVoice (const PackedVoice& packed);
PackedVoice packVoice (const VoiceData& unpacked);

} // namespace fmlib
