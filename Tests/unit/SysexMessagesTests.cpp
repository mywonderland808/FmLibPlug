#include "TestHelpers.h"
#include "sysex/SysexMessages.h"
#include "sysex/SysexParser.h"
#include <array>

using namespace fmlib;

static VoiceData makeVoice (uint8_t seed)
{
    // Build via packed domain so bank pack/unpack round-trips cleanly
    PackedVoice packed {};
    for (int i = 0; i < kPackedVoiceBytes; ++i)
        packed[static_cast<size_t> (i)] = static_cast<uint8_t> ((seed + i) & 0x7f);
    return unpackVoice (packed);
}

TEST_CASE ("makeSingleVoiceDump size and round-trip", "[sysex][messages]")
{
    const auto v = makeVoice (7);
    const auto dump = SysexMessages::makeSingleVoiceDump (v, 1);
    REQUIRE (dump.size() == 163);
    REQUIRE (dump.front() == 0xf0);
    REQUIRE (dump.back() == 0xf7);

    const auto parsed = SysexParser::parseBytes (dump);
    REQUIRE (parsed.voices.size() == 1);
    REQUIRE (parsed.voices.front().data == v);
}

TEST_CASE ("makeBankDump size and round-trip", "[sysex][messages]")
{
    std::array<VoiceData, kBankVoiceCount> bank {};
    for (int i = 0; i < kBankVoiceCount; ++i)
        bank[static_cast<size_t> (i)] = makeVoice (static_cast<uint8_t> (i + 1));

    const auto dump = SysexMessages::makeBankDump (bank, 1);
    REQUIRE (dump.size() == 4104);

    const auto parsed = SysexParser::parseBytes (dump);
    REQUIRE (parsed.voices.size() == 32);
    for (int i = 0; i < kBankVoiceCount; ++i)
        REQUIRE (parsed.voices[static_cast<size_t> (i)].data == bank[static_cast<size_t> (i)]);
}

TEST_CASE ("makeDumpRequest shape and channel clamp", "[sysex][messages]")
{
    const auto voiceReq = SysexMessages::makeDumpRequest (false, 1);
    REQUIRE (voiceReq.size() == 5);
    REQUIRE (voiceReq[0] == 0xf0);
    REQUIRE (voiceReq[1] == kYamahaId);
    REQUIRE (voiceReq[2] == 0x20); // channel 1 nibble 0
    REQUIRE (voiceReq[3] == kFormatSingleVoice);
    REQUIRE (voiceReq[4] == 0xf7);

    const auto bankReq = SysexMessages::makeDumpRequest (true, 16);
    REQUIRE (bankReq[2] == 0x2f);
    REQUIRE (bankReq[3] == kFormatVoiceBank);

    const auto clampedLo = SysexMessages::makeDumpRequest (false, 0);
    REQUIRE (clampedLo[2] == 0x20);
    const auto clampedHi = SysexMessages::makeDumpRequest (false, 99);
    REQUIRE (clampedHi[2] == 0x2f);
}
