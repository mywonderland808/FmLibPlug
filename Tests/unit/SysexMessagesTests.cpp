#include "TestHelpers.h"
#include "sysex/SysexMessages.h"
#include "sysex/SysexParser.h"
#include <array>
#include <optional>

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

TEST_CASE ("makeParameterChange layout for low and high params", "[sysex][messages][morph]")
{
    const auto low = SysexMessages::makeParameterChange (17, 0x40, 1);
    REQUIRE (low.size() == 7);
    REQUIRE (low[0] == 0xf0);
    REQUIRE (low[1] == kYamahaId);
    REQUIRE (low[2] == 0x10);
    REQUIRE (low[3] == 0x00);
    REQUIRE (low[4] == 17);
    REQUIRE (low[5] == 0x40);
    REQUIRE (low[6] == 0xf7);

    const auto high = SysexMessages::makeParameterChange (145, 0x41, 16);
    REQUIRE (high[2] == 0x1f);
    REQUIRE (high[3] == 0x01);
    REQUIRE (high[4] == (145 & 0x7f));
    REQUIRE (high[5] == 0x41);
}
