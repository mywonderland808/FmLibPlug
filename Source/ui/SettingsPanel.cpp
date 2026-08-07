#include "ui/SettingsPanel.h"

namespace fmlib
{

SettingsPanel::SettingsPanel()
{
    folderModel.owner = this;
    folderList.setModel (&folderModel);
    addAndMakeVisible (foldersLabel);
    addAndMakeVisible (folderList);
    addAndMakeVisible (addFolder);
    addAndMakeVisible (removeFolder);
    addAndMakeVisible (apply);
    addAndMakeVisible (refreshMidi);
    addAndMakeVisible (rescan);
    addAndMakeVisible (autoTag);
    addAndMakeVisible (resetTags);

    for (auto* l : { &midiInLabel, &midiOutLabel, &channelLabel, &pacingLabel, &noteLabel, &velLabel, &durLabel })
        addAndMakeVisible (*l);

    addAndMakeVisible (midiIn);
    addAndMakeVisible (midiOut);
    addAndMakeVisible (channel);
    addAndMakeVisible (pacing);
    addAndMakeVisible (auditionNote);
    addAndMakeVisible (auditionVel);
    addAndMakeVisible (auditionMs);
    addAndMakeVisible (dark);
    addAndMakeVisible (groupByBank);
    addAndMakeVisible (showFileColumns);
    addAndMakeVisible (hideDuplicates);
    addAndMakeVisible (showTooltips);
    addAndMakeVisible (checklist);

    checklist.setFont (juce::FontOptions (12.0f));
    checklist.setJustificationType (juce::Justification::topLeft);

    auto setupSlider = [] (juce::Slider& s, double min, double max, double step, const juce::String& suffix)
    {
        s.setRange (min, max, step);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        s.setTextValueSuffix (suffix);
    };

    channel.setRange (1, 16, 1);
    channel.setSliderStyle (juce::Slider::IncDecButtons);
    channel.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    setupSlider (pacing, 0, 100, 1, " ms");
    auditionNote.setRange (0, 127, 1);
    auditionNote.setSliderStyle (juce::Slider::IncDecButtons);
    auditionNote.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    auditionVel.setRange (1, 127, 1);
    auditionVel.setSliderStyle (juce::Slider::IncDecButtons);
    auditionVel.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    setupSlider (auditionMs, 50, 2000, 10, " ms");

    dark.setTooltip ("Switch between dark and light UI colours.");
    groupByBank.setTooltip ("In Bank view only: show sticky/section headers per bank file. Off = flat list of bank voices.");
    showFileColumns.setTooltip ("Show File and Folder columns in the patch list (applies immediately).");
    hideDuplicates.setTooltip ("Hide byte-identical voices (keeps the first after the current column sort). Settings-only; applies immediately.");
    showTooltips.setTooltip ("Enable or disable tooltips across the UI.");
    showTooltips.setToggleState (true, juce::dontSendNotification);
    channelLabel.setTooltip ("MIDI channel sent to the synth (1-16).");
    pacingLabel.setTooltip ("Delay after each SysEx message for unreliable interfaces.");
    noteLabel.setTooltip ("MIDI note used by the Audition button.");
    durLabel.setTooltip ("How long the audition note stays on before note-off (ms). Used live - Apply not required.");
    rescan.setTooltip ("Rescan all library folders for .syx / .dx7 files.");
    autoTag.setTooltip ("Add name-based category/mood tags. Merges with existing tags; does not remove manual tags.");
    resetTags.setTooltip ("Delete every tag in the library (manual and auto). Asks for confirmation.");

    addFolder.onClick = [this] { if (onAddFolder) onAddFolder(); };
    removeFolder.onClick = [this] { if (onRemoveFolder) onRemoveFolder(); };
    apply.onClick = [this] { if (onApply) onApply(); };
    refreshMidi.onClick = [this] { if (onRefreshMidi) onRefreshMidi(); };
    rescan.onClick = [this] { if (onRescan) onRescan(); };
    autoTag.onClick = [this] { if (onAutoTag) onAutoTag(); };
    resetTags.onClick = [this] { if (onResetTags) onResetTags(); };
    dark.onClick = [this] { if (onThemeChanged) onThemeChanged(); };
    groupByBank.onClick = [this] { if (onBrowserPrefsChanged) onBrowserPrefsChanged(); };
    showFileColumns.onClick = [this] { if (onBrowserPrefsChanged) onBrowserPrefsChanged(); };
    hideDuplicates.onClick = [this] { if (onBrowserPrefsChanged) onBrowserPrefsChanged(); };
}

void SettingsPanel::lookAndFeelChanged()
{
    // Fall back to brightness heuristic only when theme flag is unknown to us.
    const auto bg = findColour (juce::ResizableWindow::backgroundColourId);
    applyThemeColours (bg.getBrightness() <= 0.5f);
}

void SettingsPanel::applyThemeColours (bool darkTheme)
{
    const auto text = darkTheme ? juce::Colour (0xffe8eaed) : juce::Colour (0xff202124);
    const auto muted = darkTheme ? juce::Colour (0xffa8b0bb) : juce::Colour (0xff3c4043);
    const auto widget = darkTheme ? juce::Colour (0xff252a31) : juce::Colour (0xffffffff);
    const auto outline = darkTheme ? juce::Colour (0xff3a424d) : juce::Colour (0xff9aa0a6);
    const auto bg = darkTheme ? juce::Colour (0xff1a1d21) : juce::Colour (0xffe8eaed);
    const auto accent = darkTheme ? juce::Colour (0xff8ab4f8) : juce::Colour (0xff0b57d0);

    checklist.setColour (juce::Label::textColourId, text);
    foldersLabel.setColour (juce::Label::textColourId, text);
    for (auto* l : { &midiInLabel, &midiOutLabel, &channelLabel, &pacingLabel, &noteLabel, &velLabel, &durLabel })
        l->setColour (juce::Label::textColourId, text);

    for (auto* t : { &dark, &groupByBank, &showFileColumns, &hideDuplicates, &showTooltips })
        t->setColour (juce::ToggleButton::textColourId, text);

    auto paintCombo = [&] (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, widget);
        c.setColour (juce::ComboBox::buttonColourId, widget);
        c.setColour (juce::ComboBox::textColourId, text);
        c.setColour (juce::ComboBox::outlineColourId, outline);
        c.setColour (juce::ComboBox::arrowColourId, muted);
        c.setColour (juce::ComboBox::focusedOutlineColourId, accent);
    };
    paintCombo (midiIn);
    paintCombo (midiOut);

    auto paintSlider = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::backgroundColourId, bg);
        s.setColour (juce::Slider::thumbColourId, accent);
        s.setColour (juce::Slider::trackColourId, outline);
        s.setColour (juce::Slider::textBoxTextColourId, text);
        s.setColour (juce::Slider::textBoxBackgroundColourId, widget);
        s.setColour (juce::Slider::textBoxOutlineColourId, outline);
        s.setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
    };
    for (auto* s : { &channel, &pacing, &auditionNote, &auditionVel, &auditionMs })
        paintSlider (*s);

    auto paintButton = [&] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, widget);
        b.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, text);
        b.setColour (juce::TextButton::textColourOnId, text);
    };
    for (auto* b : { &addFolder, &removeFolder, &apply, &refreshMidi, &rescan, &autoTag, &resetTags })
        paintButton (*b);

    folderList.setColour (juce::ListBox::backgroundColourId, widget);
    folderList.setColour (juce::ListBox::outlineColourId, outline);
    folderList.setColour (juce::ListBox::textColourId, text);

    checklist.repaint();
    folderList.repaint();
    repaint();
}

void SettingsPanel::layoutLabeled (juce::Rectangle<int>& area, juce::Label& label, juce::Component& control, int controlW)
{
    auto row = area.removeFromTop (28);
    label.setBounds (row.removeFromLeft (170));
    if (controlW > 0)
        control.setBounds (row.removeFromLeft (controlW));
    else
        control.setBounds (row);
}

void SettingsPanel::setFolderPaths (const std::vector<std::filesystem::path>& paths)
{
    folders.clear();
    for (const auto& p : paths)
        folders.push_back (juce::String::fromUTF8 (p.string().c_str()));
    folderList.updateContent();
}

void SettingsPanel::addFolderPath (const juce::String& path)
{
    if (path.isNotEmpty())
    {
        folders.push_back (path);
        folderList.updateContent();
    }
}

void SettingsPanel::removeSelectedFolder()
{
    const int row = folderList.getSelectedRow();
    if (juce::isPositiveAndBelow (row, static_cast<int> (folders.size())))
    {
        folders.erase (folders.begin() + row);
        folderList.updateContent();
    }
}

std::vector<std::filesystem::path> SettingsPanel::getFolderPaths() const
{
    std::vector<std::filesystem::path> out;
    for (const auto& s : folders)
        out.emplace_back (s.toStdString());
    return out;
}

void SettingsPanel::setMidiLists (const juce::StringArray& inputs, const juce::StringArray& outputs)
{
    const auto inSel = midiIn.getText();
    const auto outSel = midiOut.getText();
    midiIn.clear();
    midiOut.clear();
    midiIn.addItemList (inputs, 1);
    midiOut.addItemList (outputs, 1);
    midiIn.setText (inSel, juce::dontSendNotification);
    midiOut.setText (outSel, juce::dontSendNotification);
}

void SettingsPanel::setMidiSelection (const juce::String& in, const juce::String& out)
{
    midiIn.setText (in, juce::dontSendNotification);
    midiOut.setText (out, juce::dontSendNotification);
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    foldersLabel.setBounds (r.removeFromTop (20));
    folderList.setBounds (r.removeFromTop (90));
    auto row = r.removeFromTop (28);
    addFolder.setBounds (row.removeFromLeft (100).reduced (2));
    removeFolder.setBounds (row.removeFromLeft (80).reduced (2));
    refreshMidi.setBounds (row.removeFromLeft (110).reduced (2));
    rescan.setBounds (row.removeFromLeft (120).reduced (2));
    apply.setBounds (row.removeFromRight (80).reduced (2));
    r.removeFromTop (6);
    auto tagRow = r.removeFromTop (28);
    autoTag.setBounds (tagRow.removeFromLeft (130).reduced (2));
    resetTags.setBounds (tagRow.removeFromLeft (120).reduced (2));
    r.removeFromTop (6);

    layoutLabeled (r, midiInLabel, midiIn);
    layoutLabeled (r, midiOutLabel, midiOut);
    layoutLabeled (r, channelLabel, channel, 160);
    layoutLabeled (r, pacingLabel, pacing);
    layoutLabeled (r, noteLabel, auditionNote, 160);
    layoutLabeled (r, velLabel, auditionVel, 160);
    layoutLabeled (r, durLabel, auditionMs);

    r.removeFromTop (6);
    auto toggles = r.removeFromTop (120);
    dark.setBounds (toggles.removeFromTop (24));
    groupByBank.setBounds (toggles.removeFromTop (24));
    showFileColumns.setBounds (toggles.removeFromTop (24));
    hideDuplicates.setBounds (toggles.removeFromTop (24));
    showTooltips.setBounds (toggles.removeFromTop (24));

    checklist.setBounds (r);
}

void SettingsPanel::FolderModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (owner == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (owner->folders.size())))
        return;
    g.fillAll (selected ? owner->findColour (juce::TextEditor::highlightColourId).withAlpha (0.35f)
                         : owner->findColour (juce::ListBox::backgroundColourId));
    g.setColour (owner->findColour (juce::Label::textColourId));
    g.drawText (owner->folders[static_cast<size_t> (row)], 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

} // namespace fmlib
