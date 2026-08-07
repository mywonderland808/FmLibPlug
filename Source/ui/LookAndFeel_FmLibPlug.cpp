#include "ui/LookAndFeel_FmLibPlug.h"
#include "ui/ThemePalette.h"

namespace fmlib
{

void LookAndFeel_FmLibPlug::setDark (bool dark)
{
    darkTheme = dark;
    const auto p = ThemePalette::forTheme (dark);

    setColourScheme ({
        p.bg,
        p.widget,
        p.raised,
        p.outline,
        p.text,
        dark ? juce::Colour (0xffc5cad3) : p.muted,
        p.accent,
        p.accentFill,
        p.text
    });

    setColour (juce::Label::textColourId, p.text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textWhenEditingColourId, p.text);

    setColour (juce::TextButton::buttonColourId, p.widget);
    setColour (juce::TextButton::buttonOnColourId, p.accent.withAlpha (0.35f));
    setColour (juce::TextButton::textColourOffId, p.text);
    setColour (juce::TextButton::textColourOnId, p.text);

    setColour (juce::ToggleButton::textColourId, p.text);
    setColour (juce::ToggleButton::tickColourId, p.accent);
    setColour (juce::ToggleButton::tickDisabledColourId, p.muted);

    setColour (juce::TextEditor::backgroundColourId, p.widget);
    setColour (juce::TextEditor::textColourId, p.text);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, p.text);
    setColour (juce::TextEditor::outlineColourId, p.outline);
    setColour (juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour (juce::CaretComponent::caretColourId, p.accent);

    setColour (juce::ListBox::backgroundColourId, p.widget);
    setColour (juce::ListBox::outlineColourId, p.outline);
    setColour (juce::ListBox::textColourId, p.text);

    setColour (juce::TableHeaderComponent::backgroundColourId, p.tableHeader);
    setColour (juce::TableHeaderComponent::textColourId, p.text);
    setColour (juce::TableHeaderComponent::outlineColourId, p.outline);

    setColour (juce::ComboBox::backgroundColourId, p.widget);
    setColour (juce::ComboBox::buttonColourId, p.widget);
    setColour (juce::ComboBox::textColourId, p.text);
    setColour (juce::ComboBox::outlineColourId, p.outline);
    setColour (juce::ComboBox::arrowColourId, p.muted);
    setColour (juce::ComboBox::focusedOutlineColourId, p.accent);

    setColour (juce::Slider::backgroundColourId, p.bg);
    setColour (juce::Slider::textBoxTextColourId, p.text);
    setColour (juce::Slider::textBoxBackgroundColourId, p.widget);
    setColour (juce::Slider::textBoxOutlineColourId, p.outline);
    setColour (juce::Slider::textBoxHighlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::Slider::thumbColourId, p.accent);
    setColour (juce::Slider::trackColourId, p.outline);
    setColour (juce::Slider::rotarySliderFillColourId, p.accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, p.outline);

    setColour (juce::PopupMenu::backgroundColourId, p.widget);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.text);
    setColour (juce::PopupMenu::headerTextColourId, p.muted);

    setColour (juce::TooltipWindow::backgroundColourId, p.tipBg);
    setColour (juce::TooltipWindow::textColourId, p.text);
    setColour (juce::TooltipWindow::outlineColourId, p.outline);

    setColour (juce::AlertWindow::backgroundColourId, p.bg);
    setColour (juce::AlertWindow::textColourId, p.text);
    setColour (juce::AlertWindow::outlineColourId, p.outline);

    setColour (juce::ResizableWindow::backgroundColourId, p.bg);
    setColour (juce::ScrollBar::thumbColourId, p.muted);
    setColour (juce::ScrollBar::backgroundColourId, p.bg);
}

} // namespace fmlib
