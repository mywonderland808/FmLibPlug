#include "sysex/VoiceMorpher.h"
#include "sysex/MorphLocks.h"
#include "sysex/Dx7Formats.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

static void setName (VoiceData& v, const char* name10)
{
    for (int i = 0; i < kNameLength; ++i)
        v[static_cast<size_t> (145 + i)] = static_cast<uint8_t> (name10[i] ? name10[i] : ' ');
}

TEST_CASE ("VoiceMorpher corners and midpoint", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    a[0] = 0;
    b[0] = 100;
    c[0] = 0;
    d[0] = 100;
    setName (a, "Piano     ");
    setName (b, "Noise     ");
    setName (c, "Guitar    ");
    setName (d, "Pad       ");

    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 0.0f, 0.0f)[0] == 0);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f)[0] == 100);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 0.0f, 1.0f)[0] == 0);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 1.0f, 1.0f)[0] == 100);
    const auto mid = VoiceMorpher::morph4 (a, b, c, d, 0.5f, 0.5f);
    REQUIRE (mid[0] == 50);
    REQUIRE (voiceNameFromData (mid) == "PNGP-50:50");
}

TEST_CASE ("VoiceMorpher morph name encodes corners and pad percent", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "Alpha     ");
    setName (b, "Bravo     ");
    setName (c, "Charlie   ");
    setName (d, "Delta     ");

    REQUIRE (voiceNameFromData (VoiceMorpher::morph4 (a, b, c, d, 0.0f, 0.0f)) == "ABCD-00:00");
    REQUIRE (voiceNameFromData (VoiceMorpher::morph4 (a, b, c, d, 1.0f, 1.0f)) == "ABCD-99:99");
    REQUIRE (voiceNameFromData (VoiceMorpher::morph4 (a, b, c, d, -1.0f, 2.0f)) == "ABCD-00:99");
}

TEST_CASE ("VoiceMorpher lock groups keep lock-ref pad bytes", "[sysex][morph][locks]")
{
    VoiceData a {}, b {}, c {}, d {};
    a[0] = 10;   // EG rate
    a[16] = 40;  // level op1
    a[134] = 5;  // algorithm
    b[0] = 90;
    b[16] = 90;
    b[134] = 31;
    c = b;
    d = b;
    setName (a, "LockA     ");
    setName (b, "LockB     ");
    setName (c, "LockC     ");
    setName (d, "LockD     ");

    const auto unlocked = VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f);
    REQUIRE (unlocked[0] == 90);
    REQUIRE (unlocked[16] == 90);
    REQUIRE (unlocked[134] == 31);

    // Lock reference at corner A (0,0)
    const auto lockedA = VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f,
                                               morphLockEg | morphLockLevels | morphLockAlgo,
                                               0.0f, 0.0f);
    REQUIRE (lockedA[0] == 10);
    REQUIRE (lockedA[16] == 40);
    REQUIRE (lockedA[134] == 5);
    REQUIRE (voiceNameFromData (lockedA).substr (0, 4) == "LLLL");

    // Lock reference at mid-top (0.5, 0) between A and B
    const auto lockedMid = VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f,
                                                 morphLockEg, 0.5f, 0.0f);
    REQUIRE (lockedMid[0] == 50);
}

TEST_CASE ("MorphLocks never override morph name", "[sysex][morph][locks]")
{
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "REFNAME   ");
    setName (b, "OTHER     ");
    setName (c, "OTHER     ");
    setName (d, "OTHER     ");
    const auto v = VoiceMorpher::morph4 (a, b, c, d, 0.5f, 0.5f, morphLockAllGroups, 0.0f, 0.0f);
    REQUIRE (voiceNameFromData (v) == "ROOO-50:50");
}

TEST_CASE ("applyLiveNamePolicy freezes name unless updating", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "Alpha     ");
    setName (b, "Bravo     ");
    setName (c, "Charlie   ");
    setName (d, "Delta     ");

    VoiceNameBytes frozen {};
    bool frozenValid = false;

    auto at = [&] (float x, float y) {
        return VoiceMorpher::morph4 (a, b, c, d, x, y);
    };

    // Manual commit seeds and keeps ABCD-XX:YY.
    auto committed = at (0.25f, 0.75f);
    VoiceMorpher::applyLiveNamePolicy (committed, frozen, frozenValid, true);
    REQUIRE (frozenValid);
    REQUIRE (voiceNameFromData (committed) == "ABCD-25:74");
    REQUIRE (VoiceMorpher::nameBytesFromVoice (committed) == frozen);

    // Edge/Note (or drag) at a new pad point keeps the frozen name.
    auto autoMorph = at (0.9f, 0.1f);
    REQUIRE (voiceNameFromData (autoMorph) == "ABCD-89:10");
    VoiceMorpher::applyLiveNamePolicy (autoMorph, frozen, frozenValid, false);
    REQUIRE (voiceNameFromData (autoMorph) == "ABCD-25:74");
    REQUIRE (frozen == VoiceMorpher::nameBytesFromVoice (committed));

    // Another auto step still frozen.
    auto auto2 = at (0.0f, 0.0f);
    VoiceMorpher::applyLiveNamePolicy (auto2, frozen, frozenValid, false);
    REQUIRE (voiceNameFromData (auto2) == "ABCD-25:74");

    // Next manual commit updates freeze to the new pad name.
    auto nextCommit = at (1.0f, 1.0f);
    VoiceMorpher::applyLiveNamePolicy (nextCommit, frozen, frozenValid, true);
    REQUIRE (voiceNameFromData (nextCommit) == "ABCD-99:99");
    REQUIRE (frozen == VoiceMorpher::nameBytesFromVoice (nextCommit));
}

TEST_CASE ("applyLiveNamePolicy seeds freeze once when empty", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "Alpha     ");
    setName (b, "Bravo     ");
    setName (c, "Charlie   ");
    setName (d, "Delta     ");

    VoiceNameBytes frozen {};
    bool frozenValid = false;

    auto first = VoiceMorpher::morph4 (a, b, c, d, 0.5f, 0.5f);
    VoiceMorpher::applyLiveNamePolicy (first, frozen, frozenValid, false);
    REQUIRE (frozenValid);
    REQUIRE (voiceNameFromData (first) == "ABCD-50:50");

    auto second = VoiceMorpher::morph4 (a, b, c, d, 0.0f, 1.0f);
    VoiceMorpher::applyLiveNamePolicy (second, frozen, frozenValid, false);
    REQUIRE (voiceNameFromData (second) == "ABCD-50:50");
}

TEST_CASE ("applyLiveNamePolicy host-style commit vs lock-style freeze", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "Alpha     ");
    setName (b, "Bravo     ");
    setName (c, "Charlie   ");
    setName (d, "Delta     ");

    VoiceNameBytes frozen {};
    bool frozenValid = false;

    // Pad commit (or host automation with motion Off): updateName=true.
    auto hostStep = VoiceMorpher::morph4 (a, b, c, d, 0.2f, 0.4f);
    VoiceMorpher::applyLiveNamePolicy (hostStep, frozen, frozenValid, true);
    REQUIRE (voiceNameFromData (hostStep) == "ABCD-20:40");

    // Another host automation step at a new pad point still commits the name.
    auto hostStep2 = VoiceMorpher::morph4 (a, b, c, d, 0.8f, 0.3f);
    VoiceMorpher::applyLiveNamePolicy (hostStep2, frozen, frozenValid, true);
    REQUIRE (voiceNameFromData (hostStep2) == "ABCD-79:30");
    REQUIRE (frozen == VoiceMorpher::nameBytesFromVoice (hostStep2));

    // Lock / lock-ref / resume emit: updateName=false keeps freeze even if params would differ.
    auto lockEmit = VoiceMorpher::morph4 (a, b, c, d, 0.0f, 0.0f);
    VoiceMorpher::applyLiveNamePolicy (lockEmit, frozen, frozenValid, false);
    REQUIRE (voiceNameFromData (lockEmit) == "ABCD-79:30");
}

TEST_CASE ("morph4 empty-field save name still encodes pad percent", "[sysex][morph]")
{
    // Save Preset with empty name field uses morph4 directly (not the frozen live policy).
    VoiceData a {}, b {}, c {}, d {};
    setName (a, "Alpha     ");
    setName (b, "Bravo     ");
    setName (c, "Charlie   ");
    setName (d, "Delta     ");
    REQUIRE (voiceNameFromData (VoiceMorpher::morph4 (a, b, c, d, 0.5f, 0.25f)) == "ABCD-50:25");
}
