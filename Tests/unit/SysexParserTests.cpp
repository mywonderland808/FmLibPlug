#include "TestHelpers.h"
#include "sysex/SysexParser.h"
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

TEST_CASE ("empty bytes returns error", "[sysex][parser]")
{
    const auto r = SysexParser::parseBytes (nullptr, 0);
    REQUIRE (r.error == "empty");
}
