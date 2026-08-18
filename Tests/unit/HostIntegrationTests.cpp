#include "host/MorphHostState.h"
#include "host/PluginSessionState.h"
#include "library/MorphPresetXml.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("MorphHostState motion mapping and clamps", "[host][morph]")
{
    const auto off = morphMotionFromChoice (0);
    REQUIRE_FALSE (off.lfoEnabled);
    REQUIRE (off.noteJumpMode == 0);
    REQUIRE_FALSE (off.controllerNotesToCallbacksOnly);

    const auto random = morphMotionFromChoice (1);
    REQUIRE_FALSE (random.lfoEnabled);
    REQUIRE (random.noteJumpMode == 1);
    REQUIRE (random.controllerNotesToCallbacksOnly);

    const auto edges = morphMotionFromChoice (2);
    REQUIRE (edges.noteJumpMode == 2);

    const auto lfo = morphMotionFromChoice (3);
    REQUIRE (lfo.lfoEnabled);
    REQUIRE (lfo.noteJumpMode == 0);

    REQUIRE (morphMotionToChoice (true, 0) == 3);
    REQUIRE (morphMotionToChoice (false, 2) == 2);
    REQUIRE (morphMotionToChoice (false, 0) == 0);

    REQUIRE (clampMorph01 (1.5f) == Catch::Approx (1.0f));
    REQUIRE (clampMorph01 (-0.1f) == Catch::Approx (0.0f));
    REQUIRE (clampMorphLfoRate (2.0f) == Catch::Approx (1.5f));
    REQUIRE (clampMorphLfoRate (-2.0f) == Catch::Approx (-1.5f));

    REQUIRE (isMorphMotionActive (3));
    REQUIRE_FALSE (isMorphMotionActive (0));
    REQUIRE (isEdgeLfoMotion (3));
    REQUIRE_FALSE (isEdgeLfoMotion (0));

    REQUIRE (morphPositionMatchesLastPluginWrite (0.5f, 0.25f, 0.5f, 0.25f));
    REQUIRE_FALSE (morphPositionMatchesLastPluginWrite (0.5f, 0.25f, 0.51f, 0.25f));

    float x = -1.0f, y = -1.0f;
    morphPadEdgePosition (0.0f, true, x, y);
    REQUIRE (x == Catch::Approx (0.0f));
    REQUIRE (y == Catch::Approx (0.0f));
    morphPadEdgePosition (0.125f, true, x, y);
    REQUIRE (x == Catch::Approx (0.5f));
    REQUIRE (y == Catch::Approx (0.0f));
    morphPadEdgePosition (0.25f, true, x, y);
    REQUIRE (x == Catch::Approx (1.0f));
    REQUIRE (y == Catch::Approx (0.0f));
}

TEST_CASE ("Edge LFO tempo division math", "[host][morph]")
{
    REQUIRE (morphLfoDivisionLabels().size() == static_cast<int> (MorphLfoDivision::count));
    REQUIRE (morphLfoBeatsPerCycle (static_cast<int> (MorphLfoDivision::quarter)) == Catch::Approx (4.0));
    REQUIRE (morphLfoBeatsPerCycle (static_cast<int> (MorphLfoDivision::eighth)) == Catch::Approx (2.0));
    REQUIRE (morphLfoBeatsPerCycle (static_cast<int> (MorphLfoDivision::quarterT)) == Catch::Approx (8.0 / 3.0));
    REQUIRE (morphLfoBeatsPerCycle (static_cast<int> (MorphLfoDivision::eighthT)) == Catch::Approx (4.0 / 3.0));
    REQUIRE (morphLfoBeatsPerCycle (static_cast<int> (MorphLfoDivision::quarterD)) == Catch::Approx (6.0));

    const float hzQuarter = morphLfoRateHzFromTempo (120.0, static_cast<int> (MorphLfoDivision::quarter), 0.25f);
    REQUIRE (hzQuarter == Catch::Approx (0.5f));
    REQUIRE (morphLfoRateHzFromTempo (120.0, static_cast<int> (MorphLfoDivision::quarter), -0.25f)
             == Catch::Approx (-0.5f));
    REQUIRE (morphLfoRateHzFromTempo (120.0, static_cast<int> (MorphLfoDivision::quarter), 0.0f) == Catch::Approx (0.0f));

    REQUIRE (morphLfoPhaseFromPpq (4.0, static_cast<int> (MorphLfoDivision::quarter), 0.25f) == Catch::Approx (1.0f));
    REQUIRE (morphLfoPhaseFromPpq (4.0, static_cast<int> (MorphLfoDivision::quarter), -0.25f)
             == Catch::Approx (-1.0f));
    REQUIRE (morphLfoPhaseFromPpq (4.0, static_cast<int> (MorphLfoDivision::half), 0.25f) == Catch::Approx (0.5f));
}

TEST_CASE ("PluginSessionState XML round-trip", "[host][session]")
{
    PluginSessionState state;
    state.stateVersion = kPluginSessionStateVersion;
    state.apvtsState = juce::ValueTree ("Params");
    state.apvtsState.setProperty ("morphX", 0.42, nullptr);
    state.liveMorph.name = "Session";
    state.liveMorph.a[0] = 7;
    state.liveMorph.nameA = "CornerA";
    state.liveMorph.posX = 0.2f;
    state.liveMorph.posY = 0.8f;
    state.liveMorph.lockRefX = 0.1f;
    state.liveMorph.lockRefY = 0.9f;
    state.liveMorph.lockGroups = morphLockEg;

    auto xml = writePluginSessionXml (state);
    REQUIRE (xml != nullptr);

    PluginSessionState loaded;
    REQUIRE (readPluginSessionXml (*xml, loaded));
    REQUIRE (loaded.stateVersion == kPluginSessionStateVersion);
    REQUIRE (static_cast<double> (loaded.apvtsState.getProperty ("morphX")) == Catch::Approx (0.42));
    REQUIRE (loaded.liveMorph.name == "Session");
    REQUIRE (loaded.liveMorph.a[0] == 7);
    REQUIRE (loaded.liveMorph.nameA == "CornerA");
    REQUIRE (loaded.liveMorph.posX == Catch::Approx (0.2f));
    REQUIRE (loaded.liveMorph.lockRefX == Catch::Approx (0.1f));
    REQUIRE (loaded.liveMorph.lockGroups == morphLockEg);
}

TEST_CASE ("PluginSessionState ignores unknown future version", "[host][session]")
{
    auto xml = std::make_unique<juce::XmlElement> ("FmLibPlugState");
    xml->setAttribute ("stateVersion", 99);
    PluginSessionState loaded;
    REQUIRE_FALSE (readPluginSessionXml (*xml, loaded));
}

TEST_CASE ("MorphPresetXml preset element round-trip", "[library][morph]")
{
    MorphPreset p;
    p.name = "Host";
    p.a[0] = 11;
    p.nameA = "A";
    p.posX = 0.33f;
    p.lockRefY = 0.66f;

    juce::XmlElement el ("preset");
    writeMorphPresetElement (el, p, true);

    MorphPreset out;
    REQUIRE (readMorphPresetElement (el, out));
    REQUIRE (out.name == "Host");
    REQUIRE (out.a[0] == 11);
    REQUIRE (out.nameA == "A");
    REQUIRE (out.posX == Catch::Approx (0.33f));
    REQUIRE (out.lockRefY == Catch::Approx (0.66f));
}
