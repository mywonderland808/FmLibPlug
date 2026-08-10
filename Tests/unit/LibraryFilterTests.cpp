#include "TestHelpers.h"
#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include "library/TagStore.h"
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
        REQUIRE_FALSE (q.favoritesOnly);
        REQUIRE (q.orGroups.size() == 1);
        REQUIRE (q.orGroups[0].atoms.size() == 3);
        REQUIRE (q.orGroups[0].atoms[0].kind == LibraryFilterAtom::Kind::text);
        REQUIRE (q.orGroups[0].atoms[0].value == "foo");
        REQUIRE (q.orGroups[0].atoms[1].kind == LibraryFilterAtom::Kind::favorites);
        REQUIRE (q.orGroups[0].atoms[2].kind == LibraryFilterAtom::Kind::text);
        REQUIRE (q.orGroups[0].atoms[2].value == "bar");
    }
    {
        const auto q = LibraryFilter::parse ("star: brass", false);
        REQUIRE_FALSE (q.favoritesOnly);
        REQUIRE (q.orGroups[0].atoms[0].kind == LibraryFilterAtom::Kind::favorites);
        REQUIRE (q.orGroups[0].atoms[1].value == "brass");
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

TEST_CASE ("LibraryFilter dupe: OR fav: is union not intersection", "[library][filter]")
{
    FavoritesStore favs;
    favs.setFavorite (2, true);
    std::vector<PatchEntry> all {
        entryWith ("A1", "a.syx", 1),
        entryWith ("A2", "b.syx", 1),
        entryWith ("FavSolo", "c.syx", 2),
        entryWith ("Other", "d.syx", 3)
    };
    const auto q = LibraryFilter::parse ("dupe: OR fav:", false);
    REQUIRE (q.duplicatesOnly);
    REQUIRE_FALSE (q.favoritesOnly);
    const auto out = LibraryFilter::apply (std::move (all), q, favs);
    REQUIRE (out.size() == 3);
    REQUIRE (out[0].contentId == 1);
    REQUIRE (out[1].contentId == 1);
    REQUIRE (out[2].contentId == 2);
}

TEST_CASE ("LibraryFilter uppercase AND/OR only; trailing ops ignored", "[library][filter]")
{
    FavoritesStore favs;
    TagStore tags;
    tags.addTag (1, "bass");
    tags.addTag (2, "analog");
    tags.addTag (3, "hard");
    std::vector<PatchEntry> all {
        entryWith ("BassVoice", "a.syx", 1),
        entryWith ("AnalogVoice", "b.syx", 2),
        entryWith ("HardVoice", "c.syx", 3)
    };

    {
        const auto q = LibraryFilter::parse ("tag:bass OR", false);
        REQUIRE (q.orGroups.size() == 1);
        const auto out = LibraryFilter::apply (all, q, favs, &tags);
        REQUIRE (out.size() == 1);
        REQUIRE (out.front().contentId == 1);
    }
    {
        const auto q = LibraryFilter::parse ("tag:bass AND", false);
        REQUIRE (q.orGroups.size() == 1);
        const auto out = LibraryFilter::apply (all, q, favs, &tags);
        REQUIRE (out.size() == 1);
        REQUIRE (out.front().contentId == 1);
    }
    {
        // Stray AND after OR must not become a text search for "and".
        const auto q = LibraryFilter::parse ("tag:bass OR AND tag:analog", false);
        REQUIRE (q.orGroups.size() == 2);
        const auto out = LibraryFilter::apply (all, q, favs, &tags);
        REQUIRE (out.size() == 2);
        REQUIRE (out[0].contentId == 1);
        REQUIRE (out[1].contentId == 2);
    }
    {
        // Lowercase "and" / "or" are normal search text, not operators.
        const auto q = LibraryFilter::parse ("hard and metal", false);
        REQUIRE (q.orGroups.size() == 1);
        REQUIRE (q.orGroups[0].atoms.size() == 1);
        REQUIRE (q.orGroups[0].atoms[0].kind == LibraryFilterAtom::Kind::text);
        REQUIRE (q.orGroups[0].atoms[0].value == "hard and metal");
    }
}

TEST_CASE ("LibraryFilter toggleAndTagToken", "[library][filter][tags]")
{
    REQUIRE (LibraryFilter::toggleAndTagToken ("", "Bass") == "tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("  ", "pad") == "tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("piano", "bass") == "tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass", "Bass") == "");
    REQUIRE (LibraryFilter::toggleAndTagToken ("piano AND tag:bass", "bass") == "piano");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass AND tag:pad", "bass") == "tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass AND tag:pad", "lead") == "tag:lead");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass AND", "pad") == "tag:bass AND tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass OR", "pad") == "tag:bass OR tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass OR dark", "pad") == "tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("fav: tag:bass", "bass") == "fav:");
    REQUIRE (LibraryFilter::toggleAndTagToken ("brass", "") == "brass");
    // Shift+click appends with AND instead of replacing.
    using Combine = LibraryFilter::TagChipCombine;
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass", "pad", Combine::withAnd)
             == "tag:bass AND tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("piano", "bass", Combine::withAnd)
             == "piano AND tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass AND tag:pad", "pad", Combine::withAnd)
             == "tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("", "pad", Combine::withAnd) == "tag:pad");

    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass", "pad", Combine::withOr)
             == "tag:bass OR tag:pad");
    REQUIRE (LibraryFilter::toggleAndTagToken ("piano", "bass", Combine::withOr)
             == "piano OR tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:bass OR tag:pad", "pad", Combine::withOr)
             == "tag:bass");
    REQUIRE (LibraryFilter::toggleAndTagToken ("", "pad", Combine::withOr) == "tag:pad");

    // Shift+AND after OR expands to DNF: (a OR b) AND c => (a AND c) OR (b AND c)
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:warm OR tag:soft", "keys", Combine::withAnd)
             == "tag:warm AND tag:keys OR tag:soft AND tag:keys");
    // Trailing AND with multiple OR groups still expands.
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:warm OR tag:soft AND", "keys", Combine::withAnd)
             == "tag:warm AND tag:keys OR tag:soft AND tag:keys");
    // Partial presence: add the tag only to branches that lack it.
    REQUIRE (LibraryFilter::toggleAndTagToken ("tag:warm AND tag:keys OR tag:soft", "keys", Combine::withAnd)
             == "tag:warm AND tag:keys OR tag:soft AND tag:keys");
    // Present on every branch: toggle off.
    REQUIRE (LibraryFilter::toggleAndTagToken (
                 "tag:warm AND tag:keys OR tag:soft AND tag:keys", "keys", Combine::withAnd)
             == "tag:warm OR tag:soft");
}
