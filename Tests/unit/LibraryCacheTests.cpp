#include "TestHelpers.h"
#include "library/FolderScanner.h"
#include "library/LibraryCache.h"
#include <chrono>
#include <fstream>
#include <thread>
#include <juce_core/juce_core.h>

using namespace fmlib;

TEST_CASE ("LibraryCache round-trip save/load", "[library][cache]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugCacheRoundTrip");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto cachePath = (dir.getFullPathName() + "/library-cache.v1").toStdString();

    LibraryCache cache;
    LibraryCacheFileEntry entry;
    entry.fingerprint.mtimeNs = 12345;
    entry.fingerprint.sizeBytes = 163;
    PatchEntry voice;
    voice.voiceName = "TestTone";
    voice.fileName = "a.syx";
    voice.relativePath = "a.syx";
    voice.absolutePath = "/lib/a.syx";
    voice.baseFolder = "/lib";
    voice.bankSlot = -1;
    voice.contentId = 0xabcdef;
    voice.voice[0] = 42;
    voice.refreshSearchCache();
    entry.voices.push_back (voice);
    cache.upsert (voice.absolutePath, std::move (entry));

    REQUIRE (cache.saveToFile (cachePath));

    LibraryCache loaded;
    REQUIRE (loaded.loadFromFile (cachePath));
    const auto* hit = loaded.find ("/lib/a.syx");
    REQUIRE (hit != nullptr);
    REQUIRE (hit->fingerprint.mtimeNs == 12345);
    REQUIRE (hit->fingerprint.sizeBytes == 163);
    REQUIRE (hit->voices.size() == 1);
    REQUIRE (hit->voices[0].voiceName == "TestTone");
    REQUIRE (hit->voices[0].voice[0] == 42);
    REQUIRE (hit->voices[0].contentId == 0xabcdef);

    dir.deleteRecursively();
}

TEST_CASE ("FolderScanner cache hit skips re-parse", "[library][cache][scanner]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugCacheScan");
    dir.deleteRecursively();
    dir.createDirectory();

    const auto src = fixturePath ("single_163.syx");
    REQUIRE (std::filesystem::exists (src));
    const auto dest = std::filesystem::path (dir.getFullPathName().toStdString()) / "voice.syx";
    std::filesystem::copy_file (src, dest);

    const auto cachePath = std::filesystem::path (dir.getFullPathName().toStdString()) / "library-cache.v1";
    const std::vector<std::filesystem::path> bases { dir.getFullPathName().toStdString() };

    FolderScanner scanner;
    auto first = scanner.scan (bases, nullptr, nullptr, cachePath);
    REQUIRE (first.filesScanned == 1);
    REQUIRE (first.voicesFound >= 1);
    REQUIRE (first.filesFromCache == 0);
    REQUIRE (std::filesystem::exists (cachePath));

    auto second = scanner.scan (bases, nullptr, nullptr, cachePath);
    REQUIRE (second.filesScanned == 1);
    REQUIRE (second.voicesFound == first.voicesFound);
    REQUIRE (second.filesFromCache == 1);
    REQUIRE (second.entries.front().voiceName == first.entries.front().voiceName);

    // Touch file so fingerprint changes — must re-parse.
    {
        std::ofstream out (dest, std::ios::binary | std::ios::app);
        // rewriting same content by re-copy forces mtime update more portably
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (20));
    std::filesystem::copy_file (src, dest, std::filesystem::copy_options::overwrite_existing);

    auto third = scanner.scan (bases, nullptr, nullptr, cachePath);
    REQUIRE (third.filesScanned == 1);
    REQUIRE (third.filesFromCache == 0);
    REQUIRE (third.voicesFound == first.voicesFound);

    dir.deleteRecursively();
}

TEST_CASE ("LibraryCache rejects corrupt file", "[library][cache]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugCacheCorrupt");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto cachePath = (dir.getFullPathName() + "/library-cache.v1").toStdString();

    {
        std::ofstream bad (cachePath, std::ios::binary);
        bad.write ("NOPE", 4);
    }

    LibraryCache cache;
    REQUIRE_FALSE (cache.loadFromFile (cachePath));
    REQUIRE (cache.size() == 0);

    dir.deleteRecursively();
}

TEST_CASE ("LibraryCache retainUnderBases drops outside paths", "[library][cache]")
{
    LibraryCache cache;
    LibraryCacheFileEntry inBase;
    inBase.fingerprint = { 1, 100 };
    PatchEntry v;
    v.voiceName = "In";
    v.absolutePath = "/lib/a/voice.syx";
    inBase.voices.push_back (v);
    cache.upsert ("/lib/a/voice.syx", std::move (inBase));

    LibraryCacheFileEntry outside;
    outside.fingerprint = { 2, 200 };
    PatchEntry o;
    o.voiceName = "Out";
    o.absolutePath = "/other/x.syx";
    outside.voices.push_back (o);
    cache.upsert ("/other/x.syx", std::move (outside));

    cache.retainUnderBases ({ "/lib" });
    REQUIRE (cache.size() == 1);
    REQUIRE (cache.find ("/lib/a/voice.syx") != nullptr);
    REQUIRE (cache.find ("/other/x.syx") == nullptr);
}

TEST_CASE ("FolderScanner cache hit refreshes relativePath for new base", "[library][cache][scanner]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("FmLibPlugCacheRelPath");
    root.deleteRecursively();
    root.createDirectory();
    const auto sub = root.getChildFile ("sub");
    sub.createDirectory();

    const auto src = fixturePath ("single_163.syx");
    const auto dest = std::filesystem::path (sub.getFullPathName().toStdString()) / "voice.syx";
    std::filesystem::copy_file (src, dest);

    const auto cachePath = std::filesystem::path (root.getFullPathName().toStdString()) / "library-cache.v1";
    const std::vector<std::filesystem::path> bases { sub.getFullPathName().toStdString() };

    FolderScanner scanner;
    REQUIRE (scanner.scan (bases, nullptr, nullptr, cachePath).filesFromCache == 0);
    auto cached = scanner.scan (bases, nullptr, nullptr, cachePath);
    REQUIRE (cached.filesFromCache == 1);
    REQUIRE (cached.entries.front().relativePath == "voice.syx");

    root.deleteRecursively();
}
