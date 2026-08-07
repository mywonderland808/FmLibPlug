#include "library/RecentStore.h"
#include "library/LibraryFilter.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("RecentStore contentIds", "[library][recent]")
{
    RecentStore recent (8);
    recent.push ({ 1, "a", "/a" });
    recent.push ({ 2, "b", "/b" });
    const auto ids = recent.contentIds();
    REQUIRE (ids.count (1) == 1);
    REQUIRE (ids.count (2) == 1);
    REQUIRE (ids.count (3) == 0);
}

TEST_CASE ("RecentStore ring buffer", "[library][recent]")
{
    RecentStore recent (2);
    recent.push ({ 1, "a", "/a" });
    recent.push ({ 2, "b", "/b" });
    recent.push ({ 3, "c", "/c" });
    REQUIRE (recent.items().size() == 2);
    REQUIRE (recent.items().front().contentId == 3);
}

TEST_CASE ("LibraryFilter richer tokens", "[library][filter]")
{
    {
        const auto q = LibraryFilter::parse ("dupe: recent: piano", false);
        REQUIRE (q.duplicatesOnly);
        REQUIRE (q.recentOnly);
        REQUIRE (q.text == "piano");
    }
    {
        const auto q = LibraryFilter::parse (":dup bass", false);
        REQUIRE (q.duplicatesOnly);
        REQUIRE (q.text == "bass");
    }
}
