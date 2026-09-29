#include "TestHelpers.h"
#include "library/BrowserList.h"
#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include <algorithm>
#include <unordered_set>

using namespace fmlib;

static PatchMeta makeVoice (const std::string& path, int slot, const std::string& name, uint64_t id)
{
    PatchMeta e;
    e.absolutePath = path;
    e.fileName = std::filesystem::path (path).filename().string();
    e.bankSlot = slot;
    e.voiceName = name;
    e.contentId = id;
    e.refreshSearchCache();
    return e;
}

TEST_CASE ("BrowserList filterForBrowser counters bank view", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchMeta> all {
        makeVoice ("/a/bank.syx", 1, "A1", 1),
        makeVoice ("/a/bank.syx", 2, "A2", 1), // dupe contentId
        makeVoice ("/b/single.syx", -1, "Solo", 3),
    };

    const auto q = LibraryFilter::parse ("", false);
    const auto r = BrowserList::filterForBrowser (all, BrowserScope::bankFiles, q, favs);

    REQUIRE (r.stats.totalInScope == 2); // bank slots only
    REQUIRE (r.stats.shown == 2);
    REQUIRE (r.stats.duplicates == 2);   // both share contentId 1
    REQUIRE (r.voiceIndices.size() == 2);
}

TEST_CASE ("BrowserList filterForBrowser all voices includes banks and singles", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchMeta> all {
        makeVoice ("/a/bank.syx", 1, "A1", 1),
        makeVoice ("/b/single.syx", -1, "Solo", 3),
    };

    const auto q = LibraryFilter::parse ("", false);
    const auto r = BrowserList::filterForBrowser (all, BrowserScope::allVoices, q, favs);

    REQUIRE (r.stats.totalInScope == 2);
    REQUIRE (r.stats.shown == 2);
}

TEST_CASE ("BrowserList filterForBrowser :singles keeps 1-voice SysEx rows", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchMeta> all {
        makeVoice ("/a/bank.syx", 1, "A1", 1),
        makeVoice ("/b/single.syx", -1, "Solo", 3),
    };

    const auto q = LibraryFilter::parse (":singles", false);
    const auto r = BrowserList::filterForBrowser (all, BrowserScope::allVoices, q, favs);

    REQUIRE (r.stats.totalInScope == 2);
    REQUIRE (r.stats.shown == 1);
    REQUIRE (r.stats.duplicates == 0);
    REQUIRE (all[static_cast<size_t> (r.voiceIndices.front())].voiceName == "Solo");
}

TEST_CASE ("BrowserList filterForBrowser total survives move into apply", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchMeta> all {
        makeVoice ("/a/a.syx", 1, "Brass", 10),
        makeVoice ("/a/a.syx", 2, "Piano", 11),
        makeVoice ("/a/a.syx", 3, "Bass", 12),
    };

    const auto q = LibraryFilter::parse ("piano", false);
    const auto r = BrowserList::filterForBrowser (all, BrowserScope::bankFiles, q, favs);

    // Regression: must not report 0 after scoped is moved into LibraryFilter::apply.
    REQUIRE (r.stats.totalInScope == 3);
    REQUIRE (r.stats.shown == 1);
    REQUIRE (all[static_cast<size_t> (r.voiceIndices.front())].voiceName == "Piano");
}

TEST_CASE ("hideDuplicates after sort keeps first in sort order", "[library][browser][stats]")
{
    FavoritesStore favs;
    // Same contentId; after name sort ascending, "Keep" comes before "Zebra".
    std::vector<PatchMeta> all {
        makeVoice ("/b/b.syx", 1, "Zebra", 42),
        makeVoice ("/a/a.syx", 1, "Keep", 42),
        makeVoice ("/c/c.syx", 1, "Other", 7),
    };

    const auto q = LibraryFilter::parse ("", false);
    auto r = BrowserList::filterForBrowser (all, BrowserScope::bankFiles, q, favs);
    REQUIRE (r.stats.totalInScope == 3);
    REQUIRE (r.stats.duplicates == 2);

    BrowserList::applyColumnSort (all, r.voiceIndices, 2, true, false, nullptr, {});
    auto kept = LibraryFilter::keepFirstByContentId (all, std::move (r.voiceIndices));
    REQUIRE (kept.size() == 2);
    REQUIRE (all[static_cast<size_t> (kept[0])].voiceName == "Keep");
    REQUIRE (all[static_cast<size_t> (kept[1])].voiceName == "Other");
}

TEST_CASE ("BrowserList countVoiceRows ignores headers", "[library][browser][stats]")
{
    std::vector<PatchMeta> voices {
        makeVoice ("/a/a.syx", 1, "A1", 1),
        makeVoice ("/a/a.syx", 2, "A2", 2),
        makeVoice ("/b/b.syx", 1, "B1", 3),
    };
    std::vector<int> idx { 0, 1, 2 };
    idx = BrowserList::sortGroupedIndices (voices, std::move (idx));
    const auto rows = BrowserList::buildRows (voices, idx, true);
    REQUIRE (rows.size() == 5); // 2 headers + 3 voices
    REQUIRE (BrowserList::countVoiceRows (rows) == 3);
}
