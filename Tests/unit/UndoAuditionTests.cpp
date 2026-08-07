#include "sysex/AuditionMidi.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("AuditionMidi note builders clamp channel", "[sysex][audition]")
{
    const auto on = AuditionMidi::makeNoteOn (0, 60, 100);
    REQUIRE (on.size() == 3);
    REQUIRE (on[0] == 0x90); // channel 1
    REQUIRE (on[1] == 60);
    REQUIRE (on[2] == 100);

    const auto off = AuditionMidi::makeNoteOff (17, 200);
    REQUIRE (off[0] == 0x8f); // channel 16
    REQUIRE (off[1] == 127);
}
