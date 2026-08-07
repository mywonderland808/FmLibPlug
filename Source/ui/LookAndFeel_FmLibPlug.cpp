#include "ui/LookAndFeel_FmLibPlug.h"

namespace fmlib
{

void LookAndFeel_FmLibPlug::setDark (bool dark)
{
    darkTheme = dark;
    if (dark)
    {
        setColourScheme ({
            juce::Colour (0xff1a1d21),
            juce::Colour (0xff252a31),
            juce::Colour (0xff2e353e),
            juce::Colour (0xff3a424d),
            juce::Colour (0xffe8eaed),
            juce::Colour (0xffc5cad3),
            juce::Colour (0xff8ab4f8),
            juce::Colour (0xff3c4a5c),
            juce::Colour (0xffe8eaed)
        });
    }
    else
    {
        setColourScheme ({
            juce::Colour (0xffe8eaed),
            juce::Colour (0xffffffff),
            juce::Colour (0xfff1f3f4),
            juce::Colour (0xff9aa0a6),
            juce::Colour (0xff202124),
            juce::Colour (0xff3c4043),
            juce::Colour (0xff0b57d0),
            juce::Colour (0xffd2e3fc),
            juce::Colour (0xff202124)
        });
    }

    const auto muted = dark ? juce::Colour (0xffa8b0bb) : juce::Colour (0xff3c4043);
    const auto bg = dark ? juce::Colour (0xff1a1d21) : juce::Colour (0xffe8eaed);
    const auto widget = dark ? juce::Colour (0xff252a31) : juce::Colour (0xffffffff);
    const auto outline = dark ? juce::Colour (0xff3a424d) : juce::Colour (0xff9aa0a6);
    const auto accent = dark ? juce::Colour (0xff8ab4f8) : juce::Colour (0xff0b57d0);
    const auto text = dark ? juce::Colour (0xffe8eaed) : juce::Colour (0xff202124);
    const auto tipBg = dark ? juce::Colour (0xff2a3038) : juce::Colour (0xfff8f9fa);
    const auto tipText = text;

    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textWhenEditingColourId, text);

    setColour (juce::TextButton::buttonColourId, widget);
    setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.35f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);

    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, muted);

    setColour (juce::TextEditor::backgroundColourId, widget);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);

    setColour (juce::ListBox::backgroundColourId, widget);
    setColour (juce::ListBox::outlineColourId, outline);
    setColour (juce::ListBox::textColourId, text);

    setColour (juce::TableHeaderComponent::backgroundColourId, dark ? juce::Colour (0xff2a3038) : juce::Colour (0xffe4e7eb));
    setColour (juce::TableHeaderComponent::textColourId, text);
    setColour (juce::TableHeaderComponent::outlineColourId, outline);

    setColour (juce::ComboBox::backgroundColourId, widget);
    setColour (juce::ComboBox::buttonColourId, widget);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::ComboBox::focusedOutlineColourId, accent);

    setColour (juce::Slider::backgroundColourId, bg);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, widget);
    setColour (juce::Slider::textBoxOutlineColourId, outline);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, outline);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, outline);

    setColour (juce::PopupMenu::backgroundColourId, widget);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, muted);

    setColour (juce::TooltipWindow::backgroundColourId, tipBg);
    setColour (juce::TooltipWindow::textColourId, tipText);
    setColour (juce::TooltipWindow::outlineColourId, outline);

    setColour (juce::AlertWindow::backgroundColourId, bg);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);

    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::ScrollBar::thumbColourId, muted);
    setColour (juce::ScrollBar::backgroundColourId, bg);
}

} // namespace fmlib
