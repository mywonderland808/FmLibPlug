#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fmlib
{

class LookAndFeel_FmLibPlug : public juce::LookAndFeel_V4
{
public:
    void setDark (bool dark);
    bool isDark() const { return darkTheme; }

private:
    bool darkTheme = true;
};

} // namespace fmlib
