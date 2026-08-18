#include "TestHelpers.h"
#include "library/FolderScanner.h"
#include "library/PatchWriter.h"
#include <atomic>
#include <filesystem>
#include <fstream>

using namespace fmlib;
namespace fs = std::filesystem;

TEST_CASE ("FolderScanner finds syx and skips others", "[library][scanner]")
{
    const auto root = fs::temp_directory_path() / "fmlibplug_scan_test";
    fs::remove_all (root);
    const auto sub = root / "sub";
    fs::create_directories (sub);

    VoiceData v {};
    v[145] = 'A';
    REQUIRE (PatchWriter::writeSingleVoice (sub, "keep", v, 1, true).ok);

    {
        std::ofstream junk (sub / "ignore.txt");
        junk << "nope";
    }

    FolderScanner scanner;
    const auto result = scanner.scan ({ root });
    REQUIRE (result.filesScanned >= 1);
    REQUIRE (result.voicesFound >= 1);
    REQUIRE_FALSE (result.entries.empty());
    REQUIRE (result.entries.front().fileName.find ("keep") != std::string::npos);
    REQUIRE (isSingleVoiceFile (result.entries.front()));

    fs::remove_all (root);
}

TEST_CASE ("FolderScanner respects cancel", "[library][scanner]")
{
    const auto root = fs::temp_directory_path() / "fmlibplug_scan_cancel";
    fs::remove_all (root);
    fs::create_directories (root);

    VoiceData v {};
    REQUIRE (PatchWriter::writeSingleVoice (root, "a", v, 1, true).ok);

    std::atomic<bool> cancel { true };
    FolderScanner scanner;
    const auto result = scanner.scan ({ root }, &cancel);
    REQUIRE (result.filesScanned == 0);
    REQUIRE (result.voicesFound == 0);
    REQUIRE (result.entries.empty());

    fs::remove_all (root);
}
