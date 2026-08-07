#include "prefs/AppPreferences.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("AppPreferences saveToFile loadFromFile round-trip", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsTest")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();
    const auto file = dir.getChildFile ("settings.xml");

    {
        AppPreferences prefs;
        prefs.darkTheme = false;
        prefs.groupByBank = false;
        prefs.showFileColumns = true;
        prefs.midiChannel = 7;
        prefs.sysexPacingMs = 35;
        prefs.midiInputName = "InPort";
        prefs.midiOutputName = "OutPort";
        prefs.baseFolders = { "/tmp/a", "/tmp/b" };
        prefs.favoriteIds.clear();
        prefs.favoriteIds.add ("99");
        prefs.favoriteIds.add ("1001");
        prefs.saveToFile (file);
        REQUIRE (file.existsAsFile());
    }

    AppPreferences loaded;
    loaded.loadFromFile (file);
    REQUIRE_FALSE (loaded.darkTheme);
    REQUIRE_FALSE (loaded.groupByBank);
    REQUIRE (loaded.showFileColumns);
    REQUIRE (loaded.midiChannel == 7);
    REQUIRE (loaded.sysexPacingMs == 35);
    REQUIRE (loaded.midiInputName == "InPort");
    REQUIRE (loaded.midiOutputName == "OutPort");
    REQUIRE (loaded.baseFolders.size() == 2);
    REQUIRE (loaded.favoriteIds.size() == 2);
    REQUIRE (loaded.favoriteIds[0] == "99");

    dir.deleteRecursively();
}
