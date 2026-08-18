#include "TestHelpers.h"
#include "sysex/SysexParser.h"
#include <fstream>
#include <vector>

using namespace fmlib;

TEST_CASE ("parse single 163 fixture", "[sysex][parser]")
{
    const auto r = SysexParser::parseFile (fixturePath ("single_163.syx"));
    REQUIRE (r.error.empty());
    REQUIRE (r.voices.size() == 1);
    REQUIRE (r.messagesFound >= 1);
    REQUIRE (voiceNameFromData (r.voices.front().data) == "TestVoice1");
}

TEST_CASE ("concatenated 1-voice SysEx keeps bankSlot -1", "[sysex][parser]")
{
    std::ifstream in (fixturePath ("single_163.syx"), std::ios::binary);
    if (! in)
        SKIP ("fixture missing");
    std::vector<uint8_t> one ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> two;
    two.reserve (one.size() * 2);
    two.insert (two.end(), one.begin(), one.end());
    two.insert (two.end(), one.begin(), one.end());
    const auto r = SysexParser::parseBytes (two.data(), two.size());
    REQUIRE (r.voices.size() == 2);
    REQUIRE (r.voices[0].bankSlot == -1);
    REQUIRE (r.voices[1].bankSlot == -1);
}

TEST_CASE ("parse bank 4104 fixture", "[sysex][parser]")
{
    const auto r = SysexParser::parseFile (fixturePath ("bank_4104.syx"));
    REQUIRE (r.error.empty());
    REQUIRE (r.voices.size() == 32);
    REQUIRE (r.voices.front().bankSlot == 1);
    REQUIRE (r.voices.back().bankSlot == 32);
}

TEST_CASE ("parse headerless 4096 fixture", "[sysex][parser]")
{
    const auto r = SysexParser::parseFile (fixturePath ("bank_headerless_4096.syx"));
    REQUIRE (r.error.empty());
    REQUIRE (r.voices.size() == 32);
    REQUIRE (r.messagesFound == 1);
    REQUIRE (r.voices[0].bankSlot == 1);
}

TEST_CASE ("reject bad checksum single", "[sysex][parser]")
{
    const auto r = SysexParser::parseFile (fixturePath ("bad_checksum_163.syx"));
    REQUIRE (r.voices.empty());
    REQUIRE (r.skipped >= 1);
}

TEST_CASE ("truncated single is skipped", "[sysex][parser]")
{
    std::vector<uint8_t> truncated (100, 0);
    truncated[0] = 0xf0;
    truncated[1] = kYamahaId;
    truncated[3] = kFormatSingleVoice;
    truncated.back() = 0xf7;
    const auto r = SysexParser::parseBytes (truncated);
    REQUIRE (r.voices.empty());
}

TEST_CASE ("parse dexed packed 128-byte single", "[sysex][parser]")
{
    PackedVoice packed {};
    packed[118] = 'D';
    packed[119] = 'x';
    packed[120] = 'S';
    packed[121] = 'i';
    packed[122] = 'n';
    packed[123] = 'g';
    packed[124] = 'l';
    packed[125] = 'e';
    packed[126] = ' ';
    packed[127] = '1';
    const auto r = SysexParser::parseBytes (packed.data(), packed.size());
    REQUIRE (r.error.empty());
    REQUIRE (r.voices.size() == 1);
    REQUIRE (r.voices.front().bankSlot == -1);
    REQUIRE (voiceNameFromData (r.voices.front().data) == "DxSingle 1");
}

TEST_CASE ("empty bytes returns error", "[sysex][parser]")
{
    const auto r = SysexParser::parseBytes (nullptr, 0);
    REQUIRE (r.error == "empty");
}
