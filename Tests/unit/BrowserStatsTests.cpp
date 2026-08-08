#include "TestHelpers.h"
#include "library/BrowserList.h"
#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include <algorithm>
#include <unordered_set>

using namespace fmlib;

static PatchEntry makeVoice (const std::string& path, int slot, const std::string& name, uint64_t id)
{
    PatchEntry e;
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
    std::vector<PatchEntry> all {
        makeVoice ("/a/bank.syx", 1, "A1", 1),
        makeVoice ("/a/bank.syx", 2, "A2", 1), // dupe contentId
        makeVoice ("/b/single.syx", -1, "Solo", 3),
    };

    const auto q = LibraryFilter::parse ("", false);
    const auto r = BrowserList::filterForBrowser (all, true, q, favs);

    REQUIRE (r.stats.totalInScope == 2); // bank slots only
    REQUIRE (r.stats.shown == 2);
    REQUIRE (r.stats.duplicates == 2);   // both share contentId 1
    REQUIRE (r.voices.size() == 2);
}

TEST_CASE ("BrowserList filterForBrowser counters single view", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchEntry> all {
        makeVoice ("/a/bank.syx", 1, "A1", 1),
        makeVoice ("/b/single.syx", -1, "Solo", 3),
    };

    const auto q = LibraryFilter::parse ("", false);
    const auto r = BrowserList::filterForBrowser (all, false, q, favs);

    REQUIRE (r.stats.totalInScope == 1);
    REQUIRE (r.stats.shown == 1);
    REQUIRE (r.stats.duplicates == 0);
    REQUIRE (r.voices.front().voiceName == "Solo");
}

TEST_CASE ("BrowserList filterForBrowser total survives move into apply", "[library][browser][stats]")
{
    FavoritesStore favs;
    std::vector<PatchEntry> all {
        makeVoice ("/a/a.syx", 1, "Brass", 10),
        makeVoice ("/a/a.syx", 2, "Piano", 11),
        makeVoice ("/a/a.syx", 3, "Bass", 12),
    };

    const auto q = LibraryFilter::parse ("piano", false);
    const auto r = BrowserList::filterForBrowser (all, true, q, favs);

    // Regression: must not report 0 after scoped is moved into LibraryFilter::apply.
    REQUIRE (r.stats.totalInScope == 3);
    REQUIRE (r.stats.shown == 1);
    REQUIRE (r.voices.front().voiceName == "Piano");
}

TEST_CASE ("hideDuplicates after sort keeps first in sort order", "[library][browser][stats]")
{
    FavoritesStore favs;
    // Same contentId; after name sort ascending, "Keep" comes before "Zebra".
    std::vector<PatchEntry> all {
        makeVoice ("/b/b.syx", 1, "Zebra", 42),
        makeVoice ("/a/a.syx", 1, "Keep", 42),
        makeVoice ("/c/c.syx", 1, "Other", 7),
    };

    const auto q = LibraryFilter::parse ("", false);
    auto r = BrowserList::filterForBrowser (all, true, q, favs);
    REQUIRE (r.stats.totalInScope == 3);
    REQUIRE (r.stats.duplicates == 2);

    std::sort (r.voices.begin(), r.voices.end(), [] (const PatchEntry& a, const PatchEntry& b)
    {
        return a.voiceName < b.voiceName;
    });
    auto kept = LibraryFilter::keepFirstByContentId (std::move (r.voices));
    REQUIRE (kept.size() == 2);
    REQUIRE (kept[0].voiceName == "Keep");
    REQUIRE (kept[1].voiceName == "Other");
}

TEST_CASE ("BrowserList countVoiceRows ignores headers", "[library][browser][stats]")
{
    auto voices = BrowserList::sortGrouped ({
        makeVoice ("/a/a.syx", 1, "A1", 1),
        makeVoice ("/a/a.syx", 2, "A2", 2),
        makeVoice ("/b/b.syx", 1, "B1", 3),
    });
    const auto rows = BrowserList::buildRows (std::move (voices), true);
    REQUIRE (rows.size() == 5); // 2 headers + 3 voices
    REQUIRE (BrowserList::countVoiceRows (rows) == 3);
}
