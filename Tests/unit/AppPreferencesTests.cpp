#include "prefs/AppPreferences.h"
#include <catch2/catch_approx.hpp>
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
        prefs.midiControllerInputName = "CtrlIn";
        prefs.morphEmitMs = 120;
        prefs.morphReleaseGuardMs = 600;
        prefs.morphNoteSettleMs = 75;
        prefs.morphLockGroups = 5;
        prefs.morphLockRefX = 0.3f;
        prefs.morphLockRefY = 0.7f;
        prefs.morphLfoEnabled = true;
        prefs.morphLfoRateHz = -0.5f;
        prefs.morphNoteJumpMode = 2;
        prefs.morphStreamMode = 0;
        prefs.midiControllerThru = true;
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
    REQUIRE (loaded.midiControllerInputName == "CtrlIn");
    REQUIRE (loaded.morphEmitMs == 120);
    REQUIRE (loaded.morphReleaseGuardMs == 600);
    REQUIRE (loaded.morphNoteSettleMs == 75);
    REQUIRE (loaded.morphLockGroups == 5);
    // Round-trip writes morphLockSchema=2 so factory migration does not overwrite saved locks.
    REQUIRE (loaded.morphLockRefX == Catch::Approx (0.3f));
    REQUIRE (loaded.morphLockRefY == Catch::Approx (0.7f));
    REQUIRE (loaded.morphLfoEnabled);
    REQUIRE (loaded.morphLfoRateHz == Catch::Approx (-0.5f));
    REQUIRE (loaded.morphNoteJumpMode == 2);
    REQUIRE (loaded.morphStreamMode == 0);
    REQUIRE (loaded.midiControllerThru);
    REQUIRE (loaded.baseFolders.size() == 2);
    REQUIRE (loaded.favoriteIds.size() == 2);
    REQUIRE (loaded.favoriteIds[0] == "99");

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences migrates pre-schema lock defaults to EG+Levels factory set", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsMigrate")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();
    const auto file = dir.getChildFile ("settings.xml");
    file.replaceWithText ("<?xml version=\"1.0\"?><FmLibPlugSettings morphLockGroups=\"0\"/>");

    AppPreferences loaded;
    loaded.loadFromFile (file);
    REQUIRE (loaded.morphLockGroups == morphLockFactoryDefaults);

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences schema1 migrates to EG+Levels factory", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsSchema1")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();
    const auto file = dir.getChildFile ("settings.xml");
    file.replaceWithText (
        "<?xml version=\"1.0\"?><FmLibPlugSettings morphLockSchema=\"1\" morphLockGroups=\"4\"/>");

    AppPreferences loaded;
    loaded.loadFromFile (file);
    REQUIRE (loaded.morphLockGroups == morphLockFactoryDefaults);

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences maps legacy morph profiles to stream modes", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsMorphStream")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();

    auto loadWith = [&dir] (const juce::String& attributes)
    {
        const auto file = dir.getNonexistentChildFile ("settings", ".xml");
        file.replaceWithText ("<?xml version=\"1.0\"?><FmLibPlugSettings " + attributes + "/>");
        AppPreferences prefs;
        prefs.loadFromFile (file);
        return prefs.morphStreamMode;
    };

    // Legacy gated (0) and realtime (2) both become frequency-only; continuous (1) becomes all parameters.
    REQUIRE (loadWith ("morphHardwareProfile=\"0\"") == static_cast<int> (MorphStreamMode::freqOnly));
    REQUIRE (loadWith ("morphHardwareProfile=\"2\"") == static_cast<int> (MorphStreamMode::freqOnly));
    REQUIRE (loadWith ("morphHardwareProfile=\"1\"") == static_cast<int> (MorphStreamMode::allParams));
    // The new key wins when present.
    REQUIRE (loadWith ("morphHardwareProfile=\"1\" morphStreamMode=\"1\"")
             == static_cast<int> (MorphStreamMode::freqOnly));
    // Neither key -> compile-time default (freqOnly).
    REQUIRE (loadWith ("") == static_cast<int> (MorphStreamMode::freqOnly));

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences clamps morph release hold and defaults it for older settings", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsReleaseGuard")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();

    auto loadWith = [&dir] (const juce::String& attributes)
    {
        const auto file = dir.getNonexistentChildFile ("settings", ".xml");
        file.replaceWithText ("<?xml version=\"1.0\"?><FmLibPlugSettings " + attributes + "/>");
        AppPreferences prefs;
        prefs.loadFromFile (file);
        return prefs.morphReleaseGuardMs;
    };

    // Settings written before the hold existed adopt the factory value.
    REQUIRE (loadWith ("") == 250);
    REQUIRE (loadWith ("morphReleaseGuardMs=\"0\"") == 0);
    REQUIRE (loadWith ("morphReleaseGuardMs=\"800\"") == 800);
    REQUIRE (loadWith ("morphReleaseGuardMs=\"-50\"") == 0);
    REQUIRE (loadWith ("morphReleaseGuardMs=\"99999\"") == 2000);

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences clamps note morph settle and defaults it for older settings", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsNoteSettle")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();

    auto loadWith = [&dir] (const juce::String& attributes)
    {
        const auto file = dir.getNonexistentChildFile ("settings", ".xml");
        file.replaceWithText ("<?xml version=\"1.0\"?><FmLibPlugSettings " + attributes + "/>");
        AppPreferences prefs;
        prefs.loadFromFile (file);
        return prefs.morphNoteSettleMs;
    };

    REQUIRE (loadWith ("") == 40);
    REQUIRE (loadWith ("morphNoteSettleMs=\"0\"") == 0);
    REQUIRE (loadWith ("morphNoteSettleMs=\"80\"") == 80);
    REQUIRE (loadWith ("morphNoteSettleMs=\"-10\"") == 0);
    REQUIRE (loadWith ("morphNoteSettleMs=\"5000\"") == 100);

    dir.deleteRecursively();
}

TEST_CASE ("AppPreferences compiledDefaults ignores settings.xml", "[prefs]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("FmLibPlugPrefsCompiled")
                         .getNonexistentChildFile ("run", "");
    dir.createDirectory();
    // compiledDefaults must not depend on disk; values match member initializers.
    const auto def = AppPreferences::compiledDefaults();
    REQUIRE (def.sysexPacingMs == 20);
    REQUIRE (def.morphStreamMode == static_cast<int> (MorphStreamMode::freqOnly));
    REQUIRE_FALSE (def.midiControllerThru);
    REQUIRE (def.morphEmitMs == 150);
    REQUIRE (def.morphReleaseGuardMs == 250);
    REQUIRE (def.morphNoteSettleMs == 40);
    REQUIRE (def.auditionNote == 60);
    dir.deleteRecursively();
}
