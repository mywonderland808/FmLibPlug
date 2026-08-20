#include "library/TagStore.h"
#include "library/LibraryFilter.h"
#include "library/FavoritesStore.h"
#include "library/PatchEntry.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace fmlib;

TEST_CASE ("TagStore set get clear and file round-trip", "[library][tags]")
{
    TagStore store;
    store.addTag (42, "Bass");
    store.addTag (42, "bass"); // dedupe lower
    REQUIRE (store.hasTag (42, "bass"));
    REQUIRE (store.getTags (42).size() == 1);

    store.removeTag (42, "bass");
    REQUIRE (store.getTags (42).empty());
    store.addTag (42, "bass");
    REQUIRE (store.hasTag (42, "bass"));

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugTagTest");
    dir.createDirectory();
    const auto file = dir.getChildFile ("tags.xml");
    store.saveToFile (file);

    TagStore loaded;
    loaded.loadFromFile (file);
    REQUIRE (loaded.hasTag (42, "bass"));
    loaded.clearTags (42);
    REQUIRE (loaded.getTags (42).empty());
    dir.deleteRecursively();
}

TEST_CASE ("TagStore allUniqueTags sorted and unique", "[library][tags]")
{
    TagStore store;
    store.addTag (1, "pad");
    store.addTag (1, "bass");
    store.addTag (2, "Bass");
    store.addTag (2, "lead");
    const auto all = store.allUniqueTags();
    REQUIRE (all.size() == 3);
    REQUIRE (all[0] == "bass");
    REQUIRE (all[1] == "lead");
    REQUIRE (all[2] == "pad");
}

TEST_CASE ("TagStore stores single as a normal tag", "[library][tags]")
{
    TagStore store;
    store.addTag (1, "single");
    store.addTag (1, "bass");
    REQUIRE (store.hasTag (1, "single"));
    REQUIRE (store.getTags (1) == std::vector<std::string> { "single", "bass" });
    REQUIRE (store.allUniqueTags() == std::vector<std::string> { "bass", "single" });
}

TEST_CASE ("TagStore displayJoined caches and invalidates", "[library][tags]")
{
    TagStore store;
    REQUIRE (store.displayJoined (7).empty());

    store.setTags (7, { "pad", "dark" });
    REQUIRE (store.displayJoined (7) == "pad,dark");
    REQUIRE (&store.displayJoined (7) == &store.displayJoined (7));

    store.addTag (7, "bass");
    REQUIRE (store.displayJoined (7) == "pad,dark,bass");

    store.removeTag (7, "dark");
    REQUIRE (store.displayJoined (7) == "pad,bass");

    store.clearTags (7);
    REQUIRE (store.displayJoined (7).empty());
    REQUIRE (store.getTags (7).empty());
}

TEST_CASE ("LibraryFilter tag token", "[library][filter][tags]")
{
    TagStore tags;
    tags.addTag (1, "brass");
    FavoritesStore favs;
    std::vector<PatchEntry> all (1);
    all[0].contentId = 1;
    all[0].voiceName = "Horn";
    all[0].refreshSearchCache();

    const auto q = LibraryFilter::parse ("tag:brass", false);
    REQUIRE_FALSE (q.orGroups.empty());
    REQUIRE (q.orGroups[0].atoms[0].kind == LibraryFilterAtom::Kind::tag);
    REQUIRE (q.orGroups[0].atoms[0].value == "brass");
    REQUIRE (LibraryFilter::apply (all, q, favs, &tags).size() == 1);
    REQUIRE (LibraryFilter::apply (all, q, favs, nullptr).empty());
}

TEST_CASE ("LibraryFilter AND OR combinations", "[library][filter]")
{
    TagStore tags;
    tags.addTag (1, "bass");
    tags.addTag (1, "dark");
    tags.addTag (2, "pad");
    FavoritesStore favs;
    std::vector<PatchEntry> all (2);
    all[0].contentId = 1;
    all[0].voiceName = "DarkBass";
    all[0].refreshSearchCache();
    all[1].contentId = 2;
    all[1].voiceName = "Air Pad";
    all[1].refreshSearchCache();

    {
        const auto q = LibraryFilter::parse ("tag:bass AND dark", false);
        REQUIRE (LibraryFilter::apply (all, q, favs, &tags).size() == 1);
        REQUIRE (LibraryFilter::apply (all, q, favs, &tags).front().contentId == 1);
    }
    {
        const auto q = LibraryFilter::parse ("tag:bass OR tag:pad", false);
        REQUIRE (LibraryFilter::apply (all, q, favs, &tags).size() == 2);
    }
    {
        const auto q = LibraryFilter::parse ("tag:bass AND tag:pad", false);
        REQUIRE (LibraryFilter::apply (all, q, favs, &tags).empty());
    }
}
