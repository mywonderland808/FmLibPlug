#pragma once

namespace fmlib
{

/**
 * Coordinates press-and-hold Audition from multiple inputs (button + key).
 * Starts when the first input goes down; stops only when all inputs are up.
 */
struct AuditionHoldInputs
{
    enum class Action
    {
        none,
        start,
        stop
    };

    bool buttonDown = false;
    bool keyDown = false;
    bool engaged = false;

    Action setButtonDown (bool down)
    {
        buttonDown = down;
        return sync();
    }

    Action setKeyDown (bool down)
    {
        keyDown = down;
        return sync();
    }

    /** Force-release both inputs (e.g. editor teardown). */
    Action forceStop()
    {
        buttonDown = false;
        keyDown = false;
        if (! engaged)
            return Action::none;
        engaged = false;
        return Action::stop;
    }

private:
    Action sync()
    {
        const bool want = buttonDown || keyDown;
        if (want && ! engaged)
        {
            engaged = true;
            return Action::start;
        }
        if (! want && engaged)
        {
            engaged = false;
            return Action::stop;
        }
        return Action::none;
    }
};

} // namespace fmlib
