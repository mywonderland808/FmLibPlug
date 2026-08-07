#include "sysex/VoiceMorpher.h"
#include "sysex/Dx7Formats.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("VoiceMorpher corners and midpoint", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    a[0] = 0;
    b[0] = 100;
    c[0] = 0;
    d[0] = 100;
    for (int i = 0; i < kNameLength; ++i)
    {
        a[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('A');
        b[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('B');
        c[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('C');
        d[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('D');
    }

    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 0.0f, 0.0f)[0] == 0);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 1.0f, 0.0f)[0] == 100);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 0.0f, 1.0f)[0] == 0);
    REQUIRE (VoiceMorpher::morph4 (a, b, c, d, 1.0f, 1.0f)[0] == 100);
    const auto mid = VoiceMorpher::morph4 (a, b, c, d, 0.5f, 0.5f);
    REQUIRE (mid[0] == 50);
    // Midpoint is equidistant; implementation prefers A when tied.
    REQUIRE (mid[145] == 'A');
}

TEST_CASE ("VoiceMorpher clamps xy and picks nearest name", "[sysex][morph]")
{
    VoiceData a {}, b {}, c {}, d {};
    for (int i = 0; i < kNameLength; ++i)
    {
        a[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('A');
        b[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('B');
        c[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('C');
        d[static_cast<size_t> (145 + i)] = static_cast<uint8_t> ('D');
    }

    const auto clamped = VoiceMorpher::morph4 (a, b, c, d, -1.0f, 2.0f);
    REQUIRE (clamped[145] == 'C'); // (0,1) → corner C
}
