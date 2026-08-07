#include "library/FavoritesStore.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("FavoritesStore toggle set and query", "[library][favorites]")
{
    FavoritesStore store;
    REQUIRE_FALSE (store.isFavorite (42));
    store.setFavorite (42, true);
    REQUIRE (store.isFavorite (42));
    store.toggle (42);
    REQUIRE_FALSE (store.isFavorite (42));
    store.toggle (42);
    REQUIRE (store.isFavorite (42));
    store.setFavorite (42, false);
    REQUIRE_FALSE (store.isFavorite (42));
}

TEST_CASE ("FavoritesStore loadFrom and toStringArray round-trip", "[library][favorites]")
{
    FavoritesStore store;
    juce::StringArray ids;
    ids.add ("100");
    ids.add ("200");
    store.loadFrom (ids);
    REQUIRE (store.isFavorite (100));
    REQUIRE (store.isFavorite (200));
    REQUIRE_FALSE (store.isFavorite (1));

    const auto out = store.toStringArray();
    REQUIRE (out.size() == 2);
    FavoritesStore again;
    again.loadFrom (out);
    REQUIRE (again.isFavorite (100));
    REQUIRE (again.isFavorite (200));
}
