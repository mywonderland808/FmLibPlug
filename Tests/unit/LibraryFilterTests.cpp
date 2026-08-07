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
    e.refreshSearchCache();
    return e;
}

static std::string firstTextAtom (const LibraryFilterQuery& q)
{
    if (q.orGroups.empty() || q.orGroups[0].atoms.empty())
        return {};
    return q.orGroups[0].atoms[0].value;
}

TEST_CASE ("LibraryFilter parse fav and star tokens", "[library][filter]")
{
    {
        const auto q = LibraryFilter::parse ("foo fav: bar", false);
        REQUIRE (q.favoritesOnly);
        REQUIRE (firstTextAtom (q).find ("foo") != std::string::npos);
        REQUIRE (firstTextAtom (q).find ("bar") != std::string::npos);
    }
    {
        const auto q = LibraryFilter::parse ("star: brass", false);
        REQUIRE (q.favoritesOnly);
        REQUIRE (firstTextAtom (q) == "brass");
    }
    {
        const auto q = LibraryFilter::parse ("piano", true);
        REQUIRE (q.favoritesOnly);
        REQUIRE (firstTextAtom (q) == "piano");
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
        const auto q = LibraryFilter::parse ("bras", false);
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
    const auto out = LibraryFilter::keepFirstByContentId (std::move (voices));
    REQUIRE (out.size() == 2);
    REQUIRE (out[0].voiceName == "KeepMe");
    REQUIRE (out[1].voiceName == "Other");
}

TEST_CASE ("LibraryFilter dupe: keeps only duplicated contentIds", "[library][filter][dupes]")
{
    FavoritesStore favs;
    std::vector<PatchEntry> all {
        entryWith ("A1", "a.syx", 1),
        entryWith ("A2", "b.syx", 1),
        entryWith ("Solo", "c.syx", 2)
    };
    const auto q = LibraryFilter::parse ("dupe:", false);
    REQUIRE (q.duplicatesOnly);
    const auto out = LibraryFilter::apply (std::move (all), q, favs);
    REQUIRE (out.size() == 2);
    REQUIRE (out[0].contentId == 1);
    REQUIRE (out[1].contentId == 1);
}
