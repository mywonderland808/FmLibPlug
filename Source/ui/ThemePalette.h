#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fmlib
{

struct ThemePalette
{
    juce::Colour bg;
    juce::Colour widget;
    juce::Colour raised;
    juce::Colour outline;
    juce::Colour text;
    juce::Colour muted;
    juce::Colour accent;
    juce::Colour accentFill;
    juce::Colour tipBg;
    juce::Colour tableHeader;
    juce::Colour stickyHeader;

    static ThemePalette dark()
    {
        return {
            juce::Colour (0xff1a1d21),
            juce::Colour (0xff252a31),
            juce::Colour (0xff2e353e),
            juce::Colour (0xff3a424d),
            juce::Colour (0xffe8eaed),
            juce::Colour (0xffa8b0bb),
            juce::Colour (0xff8ab4f8),
            juce::Colour (0xff3c4a5c),
            juce::Colour (0xff2a3038),
            juce::Colour (0xff2a3038),
            juce::Colour (0xff3a4554),
        };
    }

    static ThemePalette light()
    {
        return {
            juce::Colour (0xffe8eaed),
            juce::Colour (0xffffffff),
            juce::Colour (0xfff1f3f4),
            juce::Colour (0xff9aa0a6),
            juce::Colour (0xff202124),
            juce::Colour (0xff3c4043),
            juce::Colour (0xff0b57d0),
            juce::Colour (0xffd2e3fc),
            juce::Colour (0xfff8f9fa),
            juce::Colour (0xffe4e7eb),
            juce::Colour (0xffc5d4eb),
        };
    }

    static ThemePalette forTheme (bool isDark) { return isDark ? dark() : light(); }
};

inline void themeAlertWindow (juce::AlertWindow& aw, juce::LookAndFeel& lnf)
{
    aw.setLookAndFeel (&lnf);
    aw.setColour (juce::AlertWindow::backgroundColourId, lnf.findColour (juce::AlertWindow::backgroundColourId));
    aw.setColour (juce::AlertWindow::textColourId, lnf.findColour (juce::AlertWindow::textColourId));
    aw.setColour (juce::AlertWindow::outlineColourId, lnf.findColour (juce::AlertWindow::outlineColourId));
    aw.sendLookAndFeelChange();
}

} // namespace fmlib
