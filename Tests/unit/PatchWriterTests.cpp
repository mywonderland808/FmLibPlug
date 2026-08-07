#include "TestHelpers.h"
#include "library/PatchWriter.h"
#include "sysex/SysexParser.h"
#include <array>
#include <filesystem>

using namespace fmlib;
namespace fs = std::filesystem;

static VoiceData seedVoice (uint8_t seed)
{
    VoiceData v {};
    for (int i = 0; i < kVoiceDataBytes; ++i)
        v[static_cast<size_t> (i)] = static_cast<uint8_t> ((seed + i) & 0x7f);
    return v;
}

TEST_CASE ("PatchWriter writeSingleVoice and re-parse", "[library][writer]")
{
    const auto dir = fs::temp_directory_path() / "fmlibplug_writer_test";
    fs::remove_all (dir);
    fs::create_directories (dir);

    const auto voice = seedVoice (9);
    const auto r = PatchWriter::writeSingleVoice (dir, "voice_a", voice, 1, false);
    REQUIRE (r.ok);
    REQUIRE (fs::file_size (r.path) == 163);

    const auto parsed = SysexParser::parseFile (r.path);
    REQUIRE (parsed.voices.size() == 1);
    REQUIRE (parsed.voices.front().data == voice);

    const auto again = PatchWriter::writeSingleVoice (dir, "voice_a", voice, 1, false);
    REQUIRE_FALSE (again.ok);
    REQUIRE (again.error == "exists");

    const auto overwrite = PatchWriter::writeSingleVoice (dir, "voice_a", voice, 1, true);
    REQUIRE (overwrite.ok);

    fs::remove_all (dir);
}

TEST_CASE ("PatchWriter sanitizes path characters", "[library][writer]")
{
    const auto dir = fs::temp_directory_path() / "fmlibplug_writer_sanitize";
    fs::remove_all (dir);
    fs::create_directories (dir);

    const auto r = PatchWriter::writeSingleVoice (dir, "bad/name:x", seedVoice (1), 1, true);
    REQUIRE (r.ok);
    REQUIRE (r.path.filename() == "bad_name_x.syx");

    fs::remove_all (dir);
}

TEST_CASE ("PatchWriter writeBank", "[library][writer]")
{
    const auto dir = fs::temp_directory_path() / "fmlibplug_writer_bank";
    fs::remove_all (dir);
    fs::create_directories (dir);

    std::array<VoiceData, kBankVoiceCount> bank {};
    for (int i = 0; i < kBankVoiceCount; ++i)
        bank[static_cast<size_t> (i)] = seedVoice (static_cast<uint8_t> (i + 2));

    const auto r = PatchWriter::writeBank (dir, "bank_a", bank, 1, true);
    REQUIRE (r.ok);
    REQUIRE (fs::file_size (r.path) == 4104);

    const auto parsed = SysexParser::parseFile (r.path);
    REQUIRE (parsed.voices.size() == 32);

    fs::remove_all (dir);
}
