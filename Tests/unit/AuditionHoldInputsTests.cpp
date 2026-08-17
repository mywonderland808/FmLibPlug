#include "util/AuditionHoldInputs.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;
using Action = AuditionHoldInputs::Action;

TEST_CASE ("AuditionHoldInputs starts on first input and stops when all release", "[ui][audition]")
{
    AuditionHoldInputs h;

    REQUIRE (h.setButtonDown (true) == Action::start);
    REQUIRE (h.engaged);
    REQUIRE (h.setKeyDown (true) == Action::none);
    REQUIRE (h.setButtonDown (false) == Action::none);
    REQUIRE (h.engaged);
    REQUIRE (h.setKeyDown (false) == Action::stop);
    REQUIRE_FALSE (h.engaged);
}

TEST_CASE ("AuditionHoldInputs key-only hold", "[ui][audition]")
{
    AuditionHoldInputs h;
    REQUIRE (h.setKeyDown (true) == Action::start);
    REQUIRE (h.setKeyDown (true) == Action::none);
    REQUIRE (h.setKeyDown (false) == Action::stop);
}

TEST_CASE ("AuditionHoldInputs forceStop clears both inputs", "[ui][audition]")
{
    AuditionHoldInputs h;
    REQUIRE (h.setButtonDown (true) == Action::start);
    REQUIRE (h.setKeyDown (true) == Action::none);
    REQUIRE (h.forceStop() == Action::stop);
    REQUIRE_FALSE (h.buttonDown);
    REQUIRE_FALSE (h.keyDown);
    REQUIRE_FALSE (h.engaged);
    REQUIRE (h.forceStop() == Action::none);
}
