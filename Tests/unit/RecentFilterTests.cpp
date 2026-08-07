#include "library/RecentStore.h"
#include "library/LibraryFilter.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("RecentStore contentIds", "[library][recent]")
{
    RecentStore recent (8);
    recent.push (1);
    recent.push (2);
    const auto ids = recent.contentIds();
    REQUIRE (ids.count (1) == 1);
    REQUIRE (ids.count (2) == 1);
    REQUIRE (ids.count (3) == 0);
}

TEST_CASE ("RecentStore ring buffer", "[library][recent]")
{
    RecentStore recent (2);
    recent.push (1);
    recent.push (2);
    recent.push (3);
    REQUIRE (recent.contentIds().size() == 2);
    REQUIRE (recent.contentIds().count (3) == 1);
    REQUIRE (recent.contentIds().count (1) == 0);
}

TEST_CASE ("RecentStore re-push and clear", "[library][recent]")
{
    RecentStore recent (4);
    recent.push (1);
    recent.push (2);
    recent.push (1);
    REQUIRE (recent.contentIds().size() == 2);
    recent.clear();
    REQUIRE (recent.contentIds().empty());
}

TEST_CASE ("LibraryFilter richer tokens", "[library][filter]")
{
    {
        const auto q = LibraryFilter::parse ("dupe: recent: piano", false);
        REQUIRE (q.duplicatesOnly);
        REQUIRE (q.recentOnly);
        REQUIRE_FALSE (q.orGroups.empty());
        REQUIRE (q.orGroups[0].atoms[0].value == "piano");
    }
    {
        const auto q = LibraryFilter::parse (":dup bass", false);
        REQUIRE (q.duplicatesOnly);
        REQUIRE (q.orGroups[0].atoms[0].value == "bass");
    }
}
