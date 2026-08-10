#include "library/MorphPresetStore.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("MorphPresetStore add remove replace and file round-trip", "[library][morph]")
{
    MorphPresetStore store;
    MorphPreset p;
    p.name = "Pad Blend";
    p.a[0] = 10;
    p.b[0] = 20;
    p.nameA = "A";
    p.nameB = "B";
    p.posX = 0.25f;
    p.posY = 0.75f;
    p.lockGroups = morphLockEg | morphLockFreq;
    p.lockRefX = 0.4f;
    p.lockRefY = 0.6f;
    store.add (p);
    REQUIRE (store.items().size() == 1);
    REQUIRE (store.items()[0].a[0] == 10);

    MorphPreset p2 = p;
    p2.name = "Updated";
    p2.a[0] = 99;
    REQUIRE (store.replaceAt (0, p2));
    REQUIRE (store.items()[0].name == "Updated");
    REQUIRE (store.items()[0].a[0] == 99);

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugMorphTest");
    dir.createDirectory();
    const auto file = dir.getChildFile ("morph.xml");
    store.saveToFile (file);

    MorphPresetStore loaded;
    loaded.loadFromFile (file);
    REQUIRE (loaded.items().size() == 1);
    REQUIRE (loaded.items()[0].name == "Updated");
    REQUIRE (loaded.items()[0].a[0] == 99);
    REQUIRE (loaded.items()[0].posX == Catch::Approx (0.25f));
    REQUIRE (loaded.items()[0].nameA == "A");
    REQUIRE (loaded.items()[0].lockGroups == (morphLockEg | morphLockFreq));
    REQUIRE (loaded.items()[0].lockRefX == Catch::Approx (0.4f));
    REQUIRE (loaded.items()[0].lockRefY == Catch::Approx (0.6f));

    loaded.removeAt (0);
    REQUIRE (loaded.items().empty());
    dir.deleteRecursively();
}
