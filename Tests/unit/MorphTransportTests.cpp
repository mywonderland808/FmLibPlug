#include "midi/MorphTransport.h"
#include "sysex/Dx7Formats.h"
#include "sysex/MorphLocks.h"
#include "sysex/SysexMessages.h"
#include "sysex/VoiceMorpher.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

static VoiceData blankVoice()
{
    return VoiceData {};
}

TEST_CASE ("MorphTransport freqOnly streams only live freq indices", "[midi][morph][transport]")
{
    MorphTransport t;
    t.setStreamMode (MorphStreamMode::freqOnly);
    t.setChannel (1);
    t.setByteBudget (70);

    VoiceData a = blankVoice();
    VoiceData b = blankVoice();
    a[18] = 1;
    b[18] = 10; // coarse: live
    a[16] = 40;
    b[16] = 90; // level: held while sounding
    a[134] = 1;
    b[134] = 20; // algo: held

    t.forceBaseline (a);
    t.setNotesSounding (true);
    t.setTarget (b, false);

    auto streamed = t.tick();
    REQUIRE_FALSE (streamed.messages.empty());
    REQUIRE_FALSE (streamed.usedFullDump);
    for (const auto& msg : streamed.messages)
    {
        REQUIRE (msg.size() == 7);
        const int p = msg[4];
        REQUIRE (p < 126);
        REQUIRE ((p % 21 == 18 || p % 21 == 19 || p % 21 == 20));
    }

    // Held parameters land once the keyboard is quiet.
    t.setNotesSounding (false);
    auto flushed = t.tick();
    REQUIRE_FALSE (flushed.messages.empty());
}

TEST_CASE ("MorphTransport allParams streams non-freq changes while sounding", "[midi][morph][transport]")
{
    MorphTransport t;
    t.setStreamMode (MorphStreamMode::allParams);
    t.setChannel (1);
    t.setByteBudget (70);

    VoiceData a = blankVoice();
    VoiceData b = blankVoice();
    a[16] = 40;
    b[16] = 90; // operator level

    t.forceBaseline (a);
    t.setNotesSounding (true);
    t.setTarget (b, false);

    auto slice = t.tick();
    REQUIRE (slice.paramsEmitted == 1);
    REQUIRE_FALSE (slice.usedFullDump);
    REQUIRE (slice.messages.front()[4] == 16);
}

TEST_CASE ("MorphTransport freqOnly needs a baseline before streaming", "[midi][morph][transport]")
{
    MorphTransport t;
    t.setStreamMode (MorphStreamMode::freqOnly);
    t.setNotesSounding (true);
    t.setTarget (blankVoice(), true);

    // While sounding with no baseline, establish one via a full dump.
    auto first = t.tick();
    REQUIRE (first.usedFullDump);
    REQUIRE_FALSE (first.messages.empty());
}

TEST_CASE ("MorphTransport clears forceCommit when target already matches", "[midi][morph][transport]")
{
    MorphTransport t;
    t.setStreamMode (MorphStreamMode::allParams);
    VoiceData v = blankVoice();
    v[16] = 40;
    t.forceBaseline (v);
    t.setTarget (v, true); // forceCommit but no diffs
    auto empty = t.tick();
    REQUIRE (empty.messages.empty());

    VoiceData next = v;
    next[16] = 50;
    t.setNotesSounding (false);
    t.setTarget (next, false);
    // Should stream params, not be stuck preferring a dump from the stale forceCommit.
    auto slice = t.tick();
    REQUIRE_FALSE (slice.messages.empty());
}

TEST_CASE ("MorphTransport prefers full dump when idle and many diffs", "[midi][morph][transport]")
{
    VoiceData a = blankVoice();
    VoiceData b = blankVoice();
    for (size_t i = 0; i < 40; ++i)
        b[i] = 40;

    auto slice = MorphTransport::buildSlice (a, b, 1, 42, false, MorphStreamMode::allParams, false);
    REQUIRE (slice.usedFullDump);
    REQUIRE (slice.messages.size() == 1);
    REQUIRE (slice.messages.front().size() == 163);
}

TEST_CASE ("MorphTransport ignores name-only diffs on the wire", "[midi][morph][transport]")
{
    MorphTransport t;
    t.setChannel (1);

    VoiceData a = blankVoice();
    VoiceData b = a;
    for (int i = 0; i < kNameLength; ++i)
        b[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('A' + i);

    t.forceBaseline (a);
    t.setNotesSounding (false);
    t.setTarget (b, false);

    // A renamed voice alone must not trigger traffic: during a morph LFO the name
    // changes every pad step and would flood the wire with full dumps.
    REQUIRE (t.pendingDiffCount() == 0);
    REQUIRE (t.tick().messages.empty());
}

TEST_CASE ("MorphTransport budget slices param changes", "[midi][morph][transport]")
{
    VoiceData a = blankVoice();
    VoiceData b = blankVoice();
    for (int i = 0; i < 12; ++i)
        b[static_cast<size_t> (i)] = 50;

    auto slice = MorphTransport::buildSlice (a, b, 1, 21, false, MorphStreamMode::allParams, false);
    // 21 bytes => 3 param changes max
    REQUIRE_FALSE (slice.usedFullDump);
    REQUIRE (slice.paramsEmitted == 3);
    REQUIRE (slice.messages.size() == 3);
}

TEST_CASE ("migrateMorphLockGroups expands legacy Freq bit", "[sysex][morph][locks]")
{
    const uint32_t legacyFreq = 1u << 2;
    const auto migrated = migrateMorphLockGroups (legacyFreq, 1);
    REQUIRE ((migrated & morphLockFreqCoarse) != 0);
    REQUIRE ((migrated & morphLockFreqFine) != 0);
}

TEST_CASE ("VoiceMorpher nearest for algorithm and osc mode", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    a[134] = 5;
    b[134] = 31;
    a[17] = 0;
    b[17] = 1;
    c = a;
    d = b;

    const auto mid = VoiceMorpher::morph4 (a, b, c, d, 0.4f, 0.0f);
    REQUIRE (mid[134] == 5); // nearest, t<0.5
    REQUIRE (mid[17] == 0);

    const auto midHi = VoiceMorpher::morph4 (a, b, c, d, 0.6f, 0.0f);
    REQUIRE (midHi[134] == 31);
    REQUIRE (midHi[17] == 1);
}

TEST_CASE ("VoiceMorpher silence guard lifts a carrier level", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    a[16] = 60;
    // others stay 0, so a morph at B would be 0 levels
    const auto dead = VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f);
    REQUIRE (dead[16] > 0);
}

TEST_CASE ("morphLockFactoryDefaults is EG+Levels", "[sysex][morph][locks]")
{
    REQUIRE ((morphLockFactoryDefaults & morphLockEg) != 0);
    REQUIRE ((morphLockFactoryDefaults & morphLockLevels) != 0);
    REQUIRE ((morphLockFactoryDefaults & morphLockAlgo) == 0);
}
