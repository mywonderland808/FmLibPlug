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
