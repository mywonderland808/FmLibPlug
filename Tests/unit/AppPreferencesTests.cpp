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
        prefs.bankFileView = false;
        prefs.groupByBank = false;
        prefs.showFileColumns = true;
        prefs.hideDuplicates = true;
        prefs.showTooltips = false;
        prefs.midiChannel = 7;
        prefs.sysexPacingMs = 35;
        prefs.auditionNote = 48;
        prefs.auditionVelocity = 90;
        prefs.auditionDurationMs = 400;
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
    REQUIRE_FALSE (loaded.bankFileView);
    REQUIRE_FALSE (loaded.groupByBank);
    REQUIRE (loaded.showFileColumns);
    REQUIRE (loaded.hideDuplicates);
    REQUIRE_FALSE (loaded.showTooltips);
    REQUIRE (loaded.midiChannel == 7);
    REQUIRE (loaded.sysexPacingMs == 35);
    REQUIRE (loaded.auditionNote == 48);
    REQUIRE (loaded.auditionVelocity == 90);
    REQUIRE (loaded.auditionDurationMs == 400);
    REQUIRE (loaded.midiInputName == "InPort");
    REQUIRE (loaded.midiOutputName == "OutPort");
    REQUIRE (loaded.baseFolders.size() == 2);
    REQUIRE (loaded.favoriteIds.size() == 2);
    REQUIRE (loaded.favoriteIds[0] == "99");

    dir.deleteRecursively();
}
