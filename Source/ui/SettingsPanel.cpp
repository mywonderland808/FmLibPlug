#include "ui/SettingsPanel.h"
#include "prefs/AppPreferences.h"
#include "ui/ThemePalette.h"
#include "util/StringUtils.h"

namespace fmlib
{

namespace
{
constexpr const char* kControllerNone = "(none)";

/** Factory values are compile-time defaults (never load settings.xml). */
const AppPreferences& factoryDefaults()
{
    static const AppPreferences defaults = AppPreferences::compiledDefaults();
    return defaults;
}
} // namespace

SettingsPanel::SettingsPanel()
{
    folderModel.owner = this;
    folderList.setModel (&folderModel);
    foldersLabel.setTooltip (
        "Library base folders. Tick/untick to enable or disable a folder (saved immediately). "
        "Click Rescan library to rebuild the preset list.");
    folderList.setTooltip (foldersLabel.getTooltip());
    addAndMakeVisible (foldersLabel);
    addAndMakeVisible (folderList);
    addAndMakeVisible (addFolder);
    addAndMakeVisible (removeFolder);
    addAndMakeVisible (refreshMidi);
    addAndMakeVisible (rescan);
    addAndMakeVisible (applyMidiPorts);
    addAndMakeVisible (autoTag);
    addAndMakeVisible (resetTags);
    addAndMakeVisible (resetDefaults);

    for (auto* l : { &midiInLabel, &midiControllerLabel, &midiOutLabel, &channelLabel, &pacingLabel, &morphEmitLabel,
                     &morphReleaseLabel, &noteSettleLabel, &morphStreamLabel, &noteLabel, &velLabel, &durLabel })
        addAndMakeVisible (*l);

    addAndMakeVisible (midiIn);
    addAndMakeVisible (midiControllerIn);
    addAndMakeVisible (midiOut);
    addAndMakeVisible (channel);
    addAndMakeVisible (pacing);
    addAndMakeVisible (morphEmit);
    addAndMakeVisible (morphRelease);
    addAndMakeVisible (noteSettle);
    addAndMakeVisible (morphStream);
    addAndMakeVisible (auditionNote);
    addAndMakeVisible (auditionVel);
    addAndMakeVisible (auditionMs);
    addAndMakeVisible (dark);
    addAndMakeVisible (showFileColumns);
    addAndMakeVisible (hideDuplicates);
    addAndMakeVisible (showTooltips);
    addAndMakeVisible (midiControllerThru);
    addAndMakeVisible (checklist);

    // Short scannable bullets (ASCII only). Morph timing detail lives on control tooltips.
    checklist.setMultiLine (true, true);
    checklist.setReadOnly (true);
    checklist.setScrollbarsShown (false);
    checklist.setCaretVisible (false);
    checklist.setPopupMenuEnabled (false);
    checklist.setFont (juce::FontOptions (12.0f));
    checklist.setText (
        "Hardware checklist\n"
        "- Memory Protect OFF before bank write (TX7 Globals Protect Off, or TX System)\n"
        "- MIDI channel matches the synth\n"
        "- Computer OUT -> synth IN; synth OUT -> Computer IN (device input)\n"
        "- Optional controller keyboard -> MIDI controller in (TX7 keys stay local)\n"
        "- Enable Forward controller MIDI to send controller/(DAW) notes to the synth\n"
        "- Prefer a proper MIDI interface for SysEx\n"
        "- Reset defaults restores morph, audition, UI, pacing, thru (not ports/tags/folders)",
        false);
    checklist.setInterceptsMouseClicks (false, false);

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
    setupSlider (morphEmit, 20, 250, 1, " ms");
    morphEmit.setValue (factoryDefaults().morphEmitMs, juce::dontSendNotification);
    setupSlider (morphRelease, 0, 2000, 10, " ms");
    morphRelease.setValue (factoryDefaults().morphReleaseGuardMs, juce::dontSendNotification);
    setupSlider (noteSettle, 0, 100, 1, " ms");
    noteSettle.setValue (factoryDefaults().morphNoteSettleMs, juce::dontSendNotification);
    // Item order must match MorphStreamMode (allParams, freqOnly).
    morphStream.addItem ("All parameters (full sweep)", 1);
    morphStream.addItem ("Frequency only (smooth pitch)", 2);
    morphStream.setSelectedItemIndex (factoryDefaults().morphStreamMode, juce::dontSendNotification);
    auditionNote.setRange (0, 127, 1);
    auditionNote.setSliderStyle (juce::Slider::IncDecButtons);
    auditionNote.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    auditionVel.setRange (1, 127, 1);
    auditionVel.setSliderStyle (juce::Slider::IncDecButtons);
    auditionVel.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    setupSlider (auditionMs, 50, 2000, 10, " ms");

    dark.setTooltip ("Switch between dark and light UI colours.");
    showFileColumns.setTooltip ("Show File and Folder columns in the patch list.");
    hideDuplicates.setTooltip ("Hide voices with the same sound parameters (ignores the 10-character name and unused bits; keeps the first after the current column sort).");
    showTooltips.setTooltip ("Enable or disable tooltips across the UI.");
    showTooltips.setToggleState (true, juce::dontSendNotification);
    midiControllerThru.setTooltip ("Forward note/CC MIDI from the controller input to MIDI out (default off). (DAW) controller notes drive morph only until this is on. When Note morph is Random/Edges, the first key of a phrase morphs then plays after a short SysEx lead-in. SysEx stays on the device input.");
    channelLabel.setTooltip ("MIDI channel sent to the synth (1-16). Applies immediately.");
    pacingLabel.setTooltip ("Delay after each SysEx message for unreliable interfaces. Applies immediately.");
    morphEmitLabel.setTooltip ("Controls morph SysEx byte budget (lower = denser updates). Pad clicks / drag-end commit a full dump when idle.");
    morphReleaseLabel.setTooltip (
        "How long morph SysEx waits after you let go of the last key. A voice dump reloads the "
        "whole patch, so one arriving while the note is still releasing clicks. Raise it for long "
        "release envelopes, lower it for a snappier pad; 0 sends immediately. Dragging the pad or "
        "triggering a note morph cancels the wait, so your own moves stay responsive.");
    noteSettleLabel.setTooltip (
        "Extra gap between a morph voice dump finishing on the wire and the note morph's delayed "
        "note-on. Transmission time is measured and always waited for; this is only how long your "
        "synth needs to apply the new voice. Raise it if note morphs still click on the attack.");
    morphStreamLabel.setTooltip (
        "Which morph parameters go to the synth while keys are held. "
        "Frequency only: oscillator coarse/fine/detune glide live, everything else lands with the next "
        "full dump once you stop playing (best on DX7 mkI / TX7). "
        "All parameters: the whole voice sweeps live, which clicks on old hardware.");
    noteLabel.setTooltip ("MIDI note used by the Audition button.");
    velLabel.setTooltip ("Velocity of the Audition note.");
    durLabel.setTooltip ("Note length for context-menu Audition. The header Audition button is press-and-hold and ignores this.");
    midiInLabel.setTooltip ("Device / SysEx input (TX7/DX7 dumps). Choose (DAW) to receive SysEx from the host MIDI bus (plugin formats only).");
    midiControllerLabel.setTooltip ("Separate input for Note On morph performance. Choose (DAW) for host MIDI notes. Leave (none) if unused.");
    midiOutLabel.setTooltip ("MIDI output for SysEx and notes. Choose (DAW) to send to the host MIDI bus (plugin formats only).");
    applyMidiPorts.setTooltip ("Apply the selected MIDI device in/out and controller input. Port changes are not used until you click this.");
    rescan.setTooltip ("Apply enabled library folders and rescan for .syx / .dx7 files.");
    autoTag.setTooltip ("Add name-based category/mood tags. Merges with existing tags; does not remove manual tags.");
    resetTags.setTooltip ("Delete every tag in the library (manual and auto). Asks for confirmation.");
    resetDefaults.setTooltip (
        "Restore factory morph, audition and UI options, plus SysEx pacing and controller thru. "
        "Leaves MIDI ports/channel, library folders and tags unchanged.");

    // Controls share the explanation shown on their label.
    for (const auto& [label, control] : std::initializer_list<std::pair<juce::Label*, juce::SettableTooltipClient*>> {
             { &channelLabel, &channel },   { &pacingLabel, &pacing },       { &morphEmitLabel, &morphEmit },
             { &noteLabel, &auditionNote }, { &velLabel, &auditionVel },     { &durLabel, &auditionMs },
             { &morphStreamLabel, &morphStream }, { &morphReleaseLabel, &morphRelease },
             { &noteSettleLabel, &noteSettle } })
        control->setTooltip (label->getTooltip());

    addFolder.onClick = [this] { if (onAddFolder) onAddFolder(); };
    removeFolder.onClick = [this]
    {
        if (onRemoveFolder)
            onRemoveFolder();
    };
    refreshMidi.onClick = [this] { if (onRefreshMidi) onRefreshMidi(); };
    rescan.onClick = [this] { if (onRescan) onRescan(); };
    applyMidiPorts.onClick = [this] { if (onApplyMidiPorts) onApplyMidiPorts(); };
    autoTag.onClick = [this] { if (onAutoTag) onAutoTag(); };
    resetTags.onClick = [this] { if (onResetTags) onResetTags(); };
    resetDefaults.onClick = [this] { resetNonMidiDefaults(); };
    dark.onClick = [this] { if (onThemeChanged) onThemeChanged(); };
    showFileColumns.onClick = [this] { if (onBrowserPrefsChanged) onBrowserPrefsChanged(); };
    hideDuplicates.onClick = [this] { if (onBrowserPrefsChanged) onBrowserPrefsChanged(); };
    showTooltips.onClick = [this] { if (onTooltipsChanged) onTooltipsChanged(); };
    midiControllerThru.onClick = [this]
    {
        if (onMorphPrefsChanged)
            onMorphPrefsChanged();
        if (onMorphPrefsPersist)
            onMorphPrefsPersist();
    };
    morphStream.onChange = [this]
    {
        if (onMorphPrefsChanged)
            onMorphPrefsChanged();
        if (onMorphPrefsPersist)
            onMorphPrefsPersist();
    };
    // Live-apply on drag; persist once on release (avoids writing settings.xml every tick).
    morphEmit.onValueChange = [this] { if (onMorphPrefsChanged) onMorphPrefsChanged(); };
    morphRelease.onValueChange = [this] { if (onMorphPrefsChanged) onMorphPrefsChanged(); };
    noteSettle.onValueChange = [this] { if (onMorphPrefsChanged) onMorphPrefsChanged(); };
    morphEmit.onDragEnd = [this] { if (onMorphPrefsPersist) onMorphPrefsPersist(); };
    morphRelease.onDragEnd = [this] { if (onMorphPrefsPersist) onMorphPrefsPersist(); };
    noteSettle.onDragEnd = [this] { if (onMorphPrefsPersist) onMorphPrefsPersist(); };
    channel.onValueChange = [this] { if (onMidiParamsChanged) onMidiParamsChanged(); };
    pacing.onValueChange = [this] { if (onMidiParamsChanged) onMidiParamsChanged(); };
    channel.onDragEnd = [this] { if (onMidiParamsPersist) onMidiParamsPersist(); };
    pacing.onDragEnd = [this] { if (onMidiParamsPersist) onMidiParamsPersist(); };
    auditionNote.onValueChange = [this] { if (onAuditionChanged) onAuditionChanged(); };
    auditionVel.onValueChange = [this] { if (onAuditionChanged) onAuditionChanged(); };
    auditionMs.onValueChange = [this] { if (onAuditionChanged) onAuditionChanged(); };
    auditionNote.onDragEnd = [this] { if (onAuditionPersist) onAuditionPersist(); };
    auditionVel.onDragEnd = [this] { if (onAuditionPersist) onAuditionPersist(); };
    auditionMs.onDragEnd = [this] { if (onAuditionPersist) onAuditionPersist(); };

    wireSliderFactoryDoubleClick();
}

void SettingsPanel::wireSliderFactoryDoubleClick()
{
    const auto& def = factoryDefaults();
    // Native JUCE behaviour works reliably for linear and inc/dec sliders.
    channel.setDoubleClickReturnValue (true, def.midiChannel);
    pacing.setDoubleClickReturnValue (true, def.sysexPacingMs);
    morphEmit.setDoubleClickReturnValue (true, def.morphEmitMs);
    morphRelease.setDoubleClickReturnValue (true, def.morphReleaseGuardMs);
    noteSettle.setDoubleClickReturnValue (true, def.morphNoteSettleMs);
    auditionNote.setDoubleClickReturnValue (true, def.auditionNote);
    auditionVel.setDoubleClickReturnValue (true, def.auditionVelocity);
    auditionMs.setDoubleClickReturnValue (true, def.auditionDurationMs);
}

void SettingsPanel::resetNonMidiDefaults()
{
    const auto& def = factoryDefaults();

    pacing.setValue (def.sysexPacingMs, juce::dontSendNotification);
    morphEmit.setValue (def.morphEmitMs, juce::dontSendNotification);
    morphRelease.setValue (def.morphReleaseGuardMs, juce::dontSendNotification);
    noteSettle.setValue (def.morphNoteSettleMs, juce::dontSendNotification);
    morphStream.setSelectedItemIndex (def.morphStreamMode, juce::dontSendNotification);
    auditionNote.setValue (def.auditionNote, juce::dontSendNotification);
    auditionVel.setValue (def.auditionVelocity, juce::dontSendNotification);
    auditionMs.setValue (def.auditionDurationMs, juce::dontSendNotification);
    dark.setToggleState (def.darkTheme, juce::dontSendNotification);
    showFileColumns.setToggleState (def.showFileColumns, juce::dontSendNotification);
    hideDuplicates.setToggleState (def.hideDuplicates, juce::dontSendNotification);
    showTooltips.setToggleState (def.showTooltips, juce::dontSendNotification);
    midiControllerThru.setToggleState (def.midiControllerThru, juce::dontSendNotification);

    if (onMidiParamsChanged)
        onMidiParamsChanged();
    if (onMidiParamsPersist)
        onMidiParamsPersist();
    if (onMorphPrefsChanged)
        onMorphPrefsChanged();
    if (onMorphPrefsPersist)
        onMorphPrefsPersist();
    if (onAuditionChanged)
        onAuditionChanged();
    if (onAuditionPersist)
        onAuditionPersist();
    if (onThemeChanged)
        onThemeChanged();
    if (onBrowserPrefsChanged)
        onBrowserPrefsChanged();
    if (onTooltipsChanged)
        onTooltipsChanged();
    if (onDefaultsReset)
        onDefaultsReset();
}

void SettingsPanel::lookAndFeelChanged()
{
    const auto bg = findColour (juce::ResizableWindow::backgroundColourId);
    applyThemeColours (bg.getBrightness() <= 0.5f);
}

void SettingsPanel::applyThemeColours (bool darkTheme)
{
    const auto p = ThemePalette::forTheme (darkTheme);

    checklist.setColour (juce::TextEditor::textColourId, p.text);
    checklist.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    checklist.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    checklist.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    checklist.setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.25f));
    foldersLabel.setColour (juce::Label::textColourId, p.text);
    for (auto* l : { &midiInLabel, &midiControllerLabel, &midiOutLabel, &channelLabel, &pacingLabel, &morphEmitLabel,
                     &morphReleaseLabel, &noteSettleLabel, &morphStreamLabel, &noteLabel, &velLabel, &durLabel })
        l->setColour (juce::Label::textColourId, p.text);

    for (auto* t : { &dark, &showFileColumns, &hideDuplicates, &showTooltips,
                     &midiControllerThru })
        t->setColour (juce::ToggleButton::textColourId, p.text);

    auto paintCombo = [&] (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, p.widget);
        c.setColour (juce::ComboBox::buttonColourId, p.widget);
        c.setColour (juce::ComboBox::textColourId, p.text);
        c.setColour (juce::ComboBox::outlineColourId, p.outline);
        c.setColour (juce::ComboBox::arrowColourId, p.muted);
        c.setColour (juce::ComboBox::focusedOutlineColourId, p.accent);
    };
    paintCombo (midiIn);
    paintCombo (midiControllerIn);
    paintCombo (midiOut);
    paintCombo (morphStream);

    auto paintSlider = [&] (juce::Slider& s)
    {
        // Visible trough so the travel range is obvious; filled track uses accent.
        s.setColour (juce::Slider::backgroundColourId, p.raised);
        s.setColour (juce::Slider::thumbColourId, p.accent);
        s.setColour (juce::Slider::trackColourId, p.accent.withMultipliedAlpha (0.65f));
        s.setColour (juce::Slider::rotarySliderFillColourId, p.accent);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, p.outline);
        s.setColour (juce::Slider::textBoxTextColourId, p.text);
        s.setColour (juce::Slider::textBoxBackgroundColourId, p.widget);
        s.setColour (juce::Slider::textBoxOutlineColourId, p.outline);
        s.setColour (juce::Slider::textBoxHighlightColourId, p.accent.withAlpha (0.35f));
    };
    for (auto* s : { &channel, &pacing, &morphEmit, &morphRelease, &noteSettle, &auditionNote, &auditionVel,
                     &auditionMs })
        paintSlider (*s);

    auto paintButton = [&] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, p.widget);
        b.setColour (juce::TextButton::buttonOnColourId, p.accent.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, p.text);
        b.setColour (juce::TextButton::textColourOnId, p.text);
    };
    for (auto* b : { &addFolder, &removeFolder, &refreshMidi, &rescan, &applyMidiPorts, &autoTag, &resetTags,
                     &resetDefaults })
        paintButton (*b);

    folderList.setColour (juce::ListBox::backgroundColourId, p.widget);
    folderList.setColour (juce::ListBox::outlineColourId, p.outline);
    folderList.setColour (juce::ListBox::textColourId, p.text);

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
        folders.push_back ({ p, true });
    folderList.updateContent();
}

void SettingsPanel::setLibraryFolders (const std::vector<LibraryFolder>& next)
{
    folders = next;
    folderList.updateContent();
}

void SettingsPanel::addFolderPath (const juce::String& path)
{
    if (path.isNotEmpty())
    {
        folders.push_back ({ path.toStdString(), true });
        folderList.updateContent();
        if (onFoldersChanged)
            onFoldersChanged();
    }
}

void SettingsPanel::removeSelectedFolder()
{
    const int row = folderList.getSelectedRow();
    if (juce::isPositiveAndBelow (row, static_cast<int> (folders.size())))
    {
        folders.erase (folders.begin() + row);
        folderList.updateContent();
        if (onFoldersChanged)
            onFoldersChanged();
    }
}

void SettingsPanel::toggleFolderEnabled (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (folders.size())))
        return;
    folders[static_cast<size_t> (row)].enabled = ! folders[static_cast<size_t> (row)].enabled;
    folderList.repaintRow (row);
    if (onFoldersChanged)
        onFoldersChanged();
}

std::vector<std::filesystem::path> SettingsPanel::getFolderPaths() const
{
    std::vector<std::filesystem::path> out;
    for (const auto& folder : folders)
        if (folder.enabled)
            out.push_back (folder.path);
    return out;
}

std::vector<LibraryFolder> SettingsPanel::getLibraryFolders() const
{
    return folders;
}

void SettingsPanel::setMidiLists (const juce::StringArray& inputs, const juce::StringArray& outputs)
{
    const auto inSel = midiIn.getText();
    const auto ctrlSel = midiControllerIn.getText();
    const auto outSel = midiOut.getText();
    midiIn.clear();
    midiControllerIn.clear();
    midiOut.clear();
    midiIn.addItemList (inputs, 1);
    midiControllerIn.addItem (kControllerNone, 1);
    midiControllerIn.addItemList (inputs, 2);
    midiOut.addItemList (outputs, 1);
    midiIn.setText (inSel, juce::dontSendNotification);
    if (ctrlSel.isEmpty() || ctrlSel == kControllerNone)
        midiControllerIn.setText (kControllerNone, juce::dontSendNotification);
    else
        midiControllerIn.setText (ctrlSel, juce::dontSendNotification);
    midiOut.setText (outSel, juce::dontSendNotification);
}

juce::String SettingsPanel::getMidiControllerIn() const
{
    const auto t = midiControllerIn.getText();
    if (t.isEmpty() || t == kControllerNone)
        return {};
    return t;
}

void SettingsPanel::setMidiSelection (const juce::String& in, const juce::String& out, const juce::String& controllerIn)
{
    midiIn.setText (in, juce::dontSendNotification);
    midiOut.setText (out, juce::dontSendNotification);
    midiControllerIn.setText (controllerIn.isEmpty() ? kControllerNone : controllerIn, juce::dontSendNotification);
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
    r.removeFromTop (6);
    auto tagRow = r.removeFromTop (28);
    autoTag.setBounds (tagRow.removeFromLeft (130).reduced (2));
    resetTags.setBounds (tagRow.removeFromLeft (120).reduced (2));
    r.removeFromTop (6);

    layoutLabeled (r, midiInLabel, midiIn);
    layoutLabeled (r, midiControllerLabel, midiControllerIn);
    layoutLabeled (r, midiOutLabel, midiOut);
    {
        auto midiApplyRow = r.removeFromTop (28);
        applyMidiPorts.setBounds (midiApplyRow.removeFromLeft (160).reduced (2));
    }
    layoutLabeled (r, channelLabel, channel, 160);
    layoutLabeled (r, pacingLabel, pacing);
    layoutLabeled (r, morphStreamLabel, morphStream);
    layoutLabeled (r, morphEmitLabel, morphEmit);
    layoutLabeled (r, morphReleaseLabel, morphRelease);
    layoutLabeled (r, noteSettleLabel, noteSettle);
    layoutLabeled (r, noteLabel, auditionNote, 160);
    layoutLabeled (r, velLabel, auditionVel, 160);
    layoutLabeled (r, durLabel, auditionMs);

    r.removeFromTop (6);
    auto toggles = r.removeFromTop (140);
    dark.setBounds (toggles.removeFromTop (24));
    showFileColumns.setBounds (toggles.removeFromTop (24));
    hideDuplicates.setBounds (toggles.removeFromTop (24));
    showTooltips.setBounds (toggles.removeFromTop (24));
    midiControllerThru.setBounds (toggles.removeFromTop (24));

    r.removeFromTop (4);
    auto resetRow = r.removeFromTop (28);
    resetDefaults.setBounds (resetRow.removeFromLeft (130).reduced (2));

    checklist.setBounds (r);
}

void SettingsPanel::FolderModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (owner == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (owner->folders.size())))
        return;
    g.fillAll (selected ? owner->findColour (juce::TextEditor::highlightColourId).withAlpha (0.35f)
                         : owner->findColour (juce::ListBox::backgroundColourId));

    const auto& folder = owner->folders[static_cast<size_t> (row)];
    constexpr int box = 14;
    const int boxY = (h - box) / 2;
    auto tick = juce::Rectangle<float> (6.0f, (float) boxY, (float) box, (float) box);
    g.setColour (owner->findColour (juce::Label::textColourId));
    g.drawRect (tick, 1.2f);
    if (folder.enabled)
    {
        g.setColour (owner->findColour (juce::ToggleButton::tickColourId));
        g.fillRect (tick.reduced (3.0f));
    }

    g.setColour (owner->findColour (juce::Label::textColourId)
                     .withMultipliedAlpha (folder.enabled ? 1.0f : 0.45f));
    g.drawText (toJuce (folder.path.string()), 28, 0, w - 32, h, juce::Justification::centredLeft, true);
}

void SettingsPanel::FolderModel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (owner == nullptr)
        return;
    if (e.x < 28)
        owner->toggleFolderEnabled (row);
}

} // namespace fmlib
