#include "TestHelpers.h"
#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include <unordered_set>

using namespace fmlib;

static PatchEntry entryWith (const std::string& name, const std::string& file, uint64_t id)
{
    PatchEntry e;
    e.voiceName = name;
    e.fileName = file;
    e.relativePath = "folder/" + file;
    e.contentId = id;
    return e;
}

TEST_CASE ("LibraryFilter parse fav and star tokens", "[library][filter]")
{
    {
        const auto q = LibraryFilter::parse ("foo fav: bar", false);
        REQUIRE (q.favoritesOnly);
        REQUIRE (q.text == "foo  bar");
    }
    {
        const auto q = LibraryFilter::parse ("star: brass", false);
        REQUIRE (q.favoritesOnly);
        REQUIRE (q.text == "brass");
    }
    {
        const auto q = LibraryFilter::parse ("piano", true);
        REQUIRE (q.favoritesOnly);
        REQUIRE (q.text == "piano");
    }
}

TEST_CASE ("LibraryFilter apply text and favorites", "[library][filter]")
{
    FavoritesStore favs;
    favs.setFavorite (100, true);

    std::vector<PatchEntry> all {
        entryWith ("Brass Pad", "brass.syx", 100),
        entryWith ("Piano", "piano.syx", 200),
        entryWith ("Strings", "strings.syx", 300)
    };

    {
        LibraryFilterQuery q;
        q.text = "bras";
        const auto out = LibraryFilter::apply (all, q, favs);
        REQUIRE (out.size() == 1);
        REQUIRE (out.front().voiceName == "Brass Pad");
    }

    {
        LibraryFilterQuery q;
        q.favoritesOnly = true;
        const auto out = LibraryFilter::apply (all, q, favs);
        REQUIRE (out.size() == 1);
        REQUIRE (out.front().contentId == 100);
    }

    {
        const auto q = LibraryFilter::parse ("fav:", false);
        const auto out = LibraryFilter::apply (all, q, favs);
        REQUIRE (out.size() == 1);
    }
}

TEST_CASE ("LibraryFilter recent: uses recentIds", "[library][filter][recent]")
{
    FavoritesStore favs;
    std::vector<PatchEntry> all {
        entryWith ("A", "a.syx", 1),
        entryWith ("B", "b.syx", 2),
        entryWith ("C", "c.syx", 3)
    };
    std::unordered_set<uint64_t> recent { 2, 3 };

    auto q = LibraryFilter::parse ("recent:", false);
    REQUIRE (q.recentOnly);

    const auto empty = LibraryFilter::apply (all, q, favs, nullptr, nullptr);
    REQUIRE (empty.empty());

    const auto out = LibraryFilter::apply (all, q, favs, nullptr, &recent);
    REQUIRE (out.size() == 2);
    REQUIRE (out[0].contentId == 2);
    REQUIRE (out[1].contentId == 3);
}

TEST_CASE ("LibraryFilter keepFirstByContentId respects input order", "[library][filter][dupes]")
{
    std::vector<PatchEntry> voices {
        entryWith ("KeepMe", "b.syx", 42),
        entryWith ("DropMe", "a.syx", 42),
        entryWith ("Other", "c.syx", 7)
    };
    const auto out = LibraryFilter::keepFirstByContentId (voices);
    REQUIRE (out.size() == 2);
    REQUIRE (out[0].voiceName == "KeepMe");
    REQUIRE (out[1].voiceName == "Other");
}
