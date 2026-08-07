#include "sysex/FormatDetect.h"
#include "sysex/FunctionDump.h"
#include "TestHelpers.h"
#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace fmlib;

TEST_CASE ("FormatDetect dexed dx7 bank fixture", "[sysex][formats][detect]")
{
    const auto path = fixturePath ("dexed_bank.dx7");
    if (! std::filesystem::exists (path))
        SKIP ("fixture missing — run GenTestFixtures");

    const auto det = FormatDetect::detectFile (path);
    REQUIRE (det.supported);
    REQUIRE ((det.kind == SysexFormatKind::dexedDx7 || det.kind == SysexFormatKind::dx7HeaderlessBank));

    std::ifstream in (path, std::ios::binary);
    std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    const auto parsed = FormatDetect::parseSupported (bytes.data(), bytes.size(), path);
    REQUIRE (parsed.voices.size() == 32);
}

TEST_CASE ("FormatDetect rejects DX7II-like dump", "[sysex][formats][detect]")
{
    const auto path = fixturePath ("dx7ii_skip.syx");
    if (! std::filesystem::exists (path))
        SKIP ("fixture missing");

    std::ifstream in (path, std::ios::binary);
    std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    const auto det = FormatDetect::detect (bytes.data(), bytes.size());
    REQUIRE (det.kind == SysexFormatKind::dx7iiOrTx802);
    REQUIRE_FALSE (det.supported);
    const auto parsed = FormatDetect::parseSupported (bytes.data(), bytes.size());
    REQUIRE (parsed.voices.empty());
}

TEST_CASE ("FunctionDump inspects function-like sysex", "[sysex][function]")
{
    const uint8_t msg[] = { 0xf0, 0x43, 0x00, 0x01, 0x00, 0x01, 0x00, 0xf7 };
    REQUIRE (FunctionDump::isFunctionOrPerformance (msg, sizeof (msg)));
    const auto info = FunctionDump::inspect (msg, sizeof (msg));
    REQUIRE (info.recognized);
}
