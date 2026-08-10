#include "PluginEditor.h"
#include "BuildId.h"
#include "library/PatchWriter.h"
#include "sysex/SysexParser.h"
#include "ui/ThemePalette.h"
#include "util/StringUtils.h"

FmLibPlugAudioProcessorEditor::FmLibPlugAudioProcessorEditor (FmLibPlugAudioProcessor& p)
    : AudioProcessorEditor (&p), plugin (p)
{
    setLookAndFeel (&lnf);
    lnf.setDark (plugin.prefs.darkTheme);

    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    subtitle.setFont (juce::FontOptions (13.0f));
    versionLabel.setFont (juce::FontOptions (12.0f));
    versionLabel.setJustificationType (juce::Justification::centredLeft);
    versionLabel.setText ("v" JucePlugin_VersionString " (" + juce::String (fmlib::buildId()) + ")",
                          juce::dontSendNotification);
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (versionLabel);
    addAndMakeVisible (settingsBtn);
    addAndMakeVisible (auditionBtn);
    addAndMakeVisible (morphBtn);
    addAndMakeVisible (status);
    addAndMakeVisible (browser);
    addAndMakeVisible (devicePanel);
    addChildComponent (settings);
    addChildComponent (morpher);
    // Theme chrome after children are parented so Settings finds our LookAndFeel.
    syncChromeColours();

    settingsBtn.setClickingTogglesState (true);
    morphBtn.setClickingTogglesState (true);
    auditionBtn.setTooltip ("Send a short audition note on the configured MIDI channel.");
    settingsBtn.setTooltip ("Open Settings.");
    morphBtn.setTooltip ("Toggle Morph pad <-> Device buffer.");

    devicePanel.setBuffer (&plugin.deviceBuffer);
    devicePanel.onStatus = [this] (const juce::String& s) { setMidiStatus (s); };
    morpher.setPresetStore (&plugin.morphPresets);
    morpher.setEmitIntervalMs (plugin.prefs.morphEmitMs);
    morpher.setDefaultLockGroups (plugin.prefs.morphLockGroups);
    morpher.setLockGroups (plugin.prefs.morphLockGroups);
    morpher.setDefaultLockRefPosition (plugin.prefs.morphLockRefX, plugin.prefs.morphLockRefY);
    morpher.setLockRefPosition (plugin.prefs.morphLockRefX, plugin.prefs.morphLockRefY);
    morpher.setLfoEnabled (plugin.prefs.morphLfoEnabled);
    morpher.setLfoRateHz (plugin.prefs.morphLfoRateHz);
    morpher.setNoteJumpMode (static_cast<fmlib::MorpherPanel::NoteJumpMode> (
        juce::jlimit (0, 2, plugin.prefs.morphNoteJumpMode)));
    plugin.midi.setControllerNotesToCallbacksOnly (plugin.prefs.morphNoteJumpMode != 0);

    plugin.library.addListener ([safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)]
    {
        // PatchLibrary::handleAsyncUpdate already runs on the message thread.
        if (safe != nullptr)
            safe->refreshLibraryView();
    });

    plugin.midi.setStatusCallback ([safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)] (const juce::String& s)
    {
        juce::MessageManager::callAsync ([safe, s]
        {
            if (safe == nullptr)
                return;
            safe->setMidiStatus (s);
        });
    });

    plugin.midi.setControllerNoteOnCallback (
        [safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)] (int note, int velocity)
        {
            juce::MessageManager::callAsync ([safe, note, velocity]
            {
                if (safe == nullptr)
                    return;
                const bool jumpOn = safe->morpher.getNoteJumpMode() != fmlib::MorpherPanel::NoteJumpMode::off;
                const bool firstOfPhrase = safe->controllerHeldNotes.empty();
                safe->controllerHeldNotes.insert (note);

                if (jumpOn)
                {
                    // Morph before the note so the new voice is on the wire when it sounds.
                    // Chord tones during lead-in join the same delayed batch.
                    if (firstOfPhrase || safe->plugin.hasPendingControllerLeadIn())
                    {
                        if (firstOfPhrase)
                        {
                            // The incoming note masks any release tail, so don't hold the dump.
                            safe->plugin.midi.cancelMorphReleaseGuard();
                            safe->morpher.applyNoteJump();
                        }
                        safe->plugin.playControllerNoteAfterDelay (note, velocity, safe->morphNoteLeadMs());
                    }
                    else
                    {
                        safe->plugin.playControllerNote (note, velocity);
                    }
                }
                else if (! safe->plugin.prefs.midiControllerThru)
                {
                    safe->plugin.playControllerNote (note, velocity);
                }
            });
        });
    plugin.midi.setControllerNoteOffCallback (
        [safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)] (int note, int)
        {
            juce::MessageManager::callAsync ([safe, note]
            {
                if (safe == nullptr)
                    return;
                const bool jumpOn = safe->morpher.getNoteJumpMode() != fmlib::MorpherPanel::NoteJumpMode::off;
                safe->controllerHeldNotes.erase (note);
                if (jumpOn || ! safe->plugin.prefs.midiControllerThru)
                    safe->plugin.releaseControllerNote (note);
                // Next jump happens on the next note-on, with its own SysEx lead-in.
            });
        });

    browser.setLoadCallback ([this] (const fmlib::PatchEntry& e, bool loadBank)
    {
        if (loadBank && e.bankSlot > 0)
        {
            auto all = plugin.library.getEntriesCopy();
            std::array<fmlib::VoiceData, fmlib::kBankVoiceCount> bank {};
            int found = 0;
            for (const auto& x : all)
            {
                if (x.absolutePath == e.absolutePath && x.bankSlot >= 1 && x.bankSlot <= fmlib::kBankVoiceCount)
                {
                    bank[static_cast<size_t> (x.bankSlot - 1)] = x.voice;
                    ++found;
                }
            }
            if (found == fmlib::kBankVoiceCount)
                plugin.midi.sendBank (bank);
            else
                plugin.sendVoice (e.voice);
        }
        else
        {
            plugin.sendVoice (e.voice);
        }
    });

    browser.setFavoriteToggleCallback ([this] (uint64_t id)
    {
        plugin.favorites.toggle (id);
        plugin.persistPreferences();
        refreshLibraryView();
    });

    browser.setOnListFocused ([this] { lastListFocus = ListFocus::browser; });
    devicePanel.onListFocused = [this] { lastListFocus = ListFocus::device; };
    devicePanel.onQueryDragVoice = [this] { return browser.getDraggedVoice(); };

    browser.setBankFileView (plugin.prefs.bankFileView);
    browser.setGroupByBank (plugin.prefs.groupByBank);
    browser.setShowFileColumns (plugin.prefs.showFileColumns);
    browser.setHideDuplicates (plugin.prefs.hideDuplicates);
    browser.setTooltipsEnabled (plugin.prefs.showTooltips);
    browser.setFavoritesOnly (plugin.prefs.favoritesOnly);
    browser.setTagFilterExpanded (plugin.prefs.tagFilterExpanded);
    browser.setOnTagFilterExpandedChanged ([this] (bool on)
    {
        plugin.prefs.tagFilterExpanded = on;
        plugin.persistPreferences();
    });
    browser.setBankFileViewChanged ([this] (bool banks)
    {
        plugin.prefs.bankFileView = banks;
        plugin.persistPreferences();
    });
    browser.setOnFavoritesOnlyChanged ([this] (bool on)
    {
        plugin.prefs.favoritesOnly = on;
        plugin.persistPreferences();
    });
    browser.setOnStatsChanged ([this] (const fmlib::BrowserStats& st)
    {
        browserStats = st;
        updateStatusBar();
    });
    browser.setOnTagsChanged ([this]
    {
        plugin.persistPreferences();
        browser.refreshTagStrip();
        browser.getTable().repaint();
    });

    settingsBtn.onClick = [this]
    {
        if (settingsBtn.getToggleState())
            showSettingsMode();
        else
            showLibraryMode();
    };
    morphBtn.onClick = [this]
    {
        if (uiMode == UiMode::morph)
            showLibraryMode(); // Morph <-> Device buffer
        else
            showMorphMode();
    };
    auditionBtn.onClick = [this]
    {
        syncAuditionPrefsFromSettings();
        if (morpher.getNoteJumpMode() != fmlib::MorpherPanel::NoteJumpMode::off)
        {
            plugin.midi.cancelMorphReleaseGuard();
            morpher.applyNoteJump();
            plugin.auditionNoteAfterDelay (morphNoteLeadMs());
        }
        else
        {
            plugin.auditionNote();
        }
        restoreListFocus();
    };

    morpher.onMorph = [this] (const fmlib::VoiceData& v, bool dragEmit)
    {
        // Drag/LFO: stream under budget. Click / drag-end / jump: commit (full dump when idle).
        plugin.midi.sendMorphVoice (v, ! dragEmit);
    };
    // Driving the pad by hand outranks the release hold: a click you asked for beats a
    // pad that ignores you for the length of the hold.
    morpher.onPadGestureStarted = [this] { plugin.midi.cancelMorphReleaseGuard(); };
    morpher.onMorphInvalidate = [this] { plugin.midi.invalidateMorphBaseline(); };
    morpher.onPresetsChanged = [this] { plugin.persistPreferences(); };
    morpher.onMorphUiPrefsChanged = [this]
    {
        // Persist LFO / note-jump and lock *defaults* (Set default updates those).
        plugin.prefs.morphLockGroups = morpher.getDefaultLockGroups();
        plugin.prefs.morphLockRefX = morpher.getDefaultLockRefX();
        plugin.prefs.morphLockRefY = morpher.getDefaultLockRefY();
        plugin.prefs.morphLfoEnabled = morpher.getLfoEnabled();
        plugin.prefs.morphLfoRateHz = morpher.getLfoRateHz();
        plugin.prefs.morphNoteJumpMode = static_cast<int> (morpher.getNoteJumpMode());
        plugin.midi.setControllerNotesToCallbacksOnly (plugin.prefs.morphNoteJumpMode != 0);
        plugin.persistPreferences();
    };
    morpher.onNoteJumpArmed = [this]
    {
        if (controllerHeldNotes.empty())
            prepareNextMorphJump();
    };
    morpher.onSaveToDeviceSlot = [this] (const fmlib::VoiceData& voice, int slot1to32)
    {
        fmlib::PatchEntry e;
        e.voice = voice;
        e.voiceName = fmlib::voiceNameFromData (voice);
        e.fileName = "morph";
        e.relativePath = "Morph";
        e.contentId = fmlib::contentIdFromVoice (voice);
        e.bankSlot = slot1to32;
        e.refreshSearchCache();
        plugin.deviceBuffer.setSlot (slot1to32 - 1, std::move (e));
        devicePanel.refreshList();
        setMidiStatus ("Saved morph preset to device buffer slot " + juce::String (slot1to32));
    };
    morpher.onRevealMorphPresetsFile = [this]
    {
        const auto f = plugin.morphPresetsFile();
        if (f.existsAsFile())
            f.revealToUser();
        else
        {
            f.getParentDirectory().createDirectory();
            f.getParentDirectory().revealToUser();
        }
        setMidiStatus ("Morph presets: " + f.getFullPathName());
    };
    morpher.onStatus = [this] (const juce::String& s) { setMidiStatus (s); };
    morpher.onRequestAssignCorner = [this] (int corner)
    {
        if (auto sel = browser.getSelectedVoice())
        {
            morpher.setCorner (corner, sel->voice, fmlib::toJuce (sel->voiceName));
            setMidiStatus ("Assigned corner " + juce::String::charToString (static_cast<juce::juce_wchar> ('A' + corner))
                                + " <- " + fmlib::toJuce (sel->voiceName));
        }
        else
        {
            setMidiStatus ("Select a voice in the library first, then Set A/B/C/D.");
        }
    };

    devicePanel.onRequest1 = [this] { plugin.midi.requestDump (false); };
    devicePanel.onRequest32 = [this] { plugin.midi.requestDump (true); };
    devicePanel.onClear = [this]
    {
        plugin.deviceBuffer.clear();
        lastDeviceVoiceCount = -1;
        devicePanel.refreshList();
    };
    devicePanel.onSave = [this] { saveDeviceBuffer(); };
    devicePanel.onSend = [this] { sendDeviceBuffer(); };
    devicePanel.onLoadVoice = [this] (const fmlib::PatchEntry& e) { plugin.sendVoice (e.voice); };

    settings.onRefreshMidi = [this]
    {
        plugin.midi.refreshDevices();
        settings.setMidiLists (plugin.midi.getInputNames(), plugin.midi.getOutputNames());
    };
    settings.onRescan = [this]
    {
        plugin.library.rescanAsync();
        setMidiStatus ("Scanning...");
    };
    settings.onThemeChanged = [this]
    {
        plugin.prefs.darkTheme = settings.getDarkTheme();
        lnf.setDark (plugin.prefs.darkTheme);
        plugin.persistPreferences();
        syncChromeColours();
        repaint();
    };
    settings.onBrowserPrefsChanged = [this]
    {
        plugin.prefs.groupByBank = settings.getGroupByBank();
        plugin.prefs.showFileColumns = settings.getShowFileColumns();
        plugin.prefs.hideDuplicates = settings.getHideDuplicates();
        browser.setGroupByBank (plugin.prefs.groupByBank);
        browser.setShowFileColumns (plugin.prefs.showFileColumns);
        browser.setHideDuplicates (plugin.prefs.hideDuplicates);
        plugin.persistPreferences();
    };
    settings.onMorphPrefsChanged = [this] { syncMorphPrefsFromSettings(); };
    settings.onMidiParamsChanged = [this] { syncMidiParamsFromSettings(); };
    settings.onApplyMidiPorts = [this] { applyMidiPortsFromSettings(); };
    settings.onFoldersChanged = [this] { syncFoldersFromSettings(); };
    settings.onAuditionChanged = [this]
    {
        syncAuditionPrefsFromSettings();
        plugin.persistPreferences();
    };
    settings.onTooltipsChanged = [this] { syncTooltipsFromSettings(); };
    settings.onAddFolder = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Add library folder");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, chooser] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.isDirectory())
                                      settings.addFolderPath (f.getFullPathName());
                              });
    };
    settings.onRemoveFolder = [this] { settings.removeSelectedFolder(); };
    settings.onAutoTag = [this]
    {
        setMidiStatus ("Auto-tagging library...");
        plugin.autoTagLibrary ([safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)]
        {
            if (safe == nullptr)
                return;
            safe->refreshLibraryView();
            safe->setMidiStatus ("Auto-tagged library (see Tags column; click Tags to edit)");
        });
    };
    settings.onResetTags = [this]
    {
        auto* aw = new juce::AlertWindow ("Reset all tags",
                                          "Delete every tag for every voice? This cannot be undone.",
                                          juce::AlertWindow::WarningIcon);
        fmlib::themeAlertWindow (*aw, lnf);
        aw->addButton ("Reset", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            std::unique_ptr<juce::AlertWindow> cleanup (aw);
            aw->setLookAndFeel (nullptr);
            if (result != 1)
                return;
            plugin.tags.clear();
            plugin.tags.saveToFile (plugin.tagsFile());
            refreshLibraryView();
            setMidiStatus ("All tags cleared");
        }));
    };

    settings.setFolderPaths (plugin.library.getBaseFolders());
    settings.setMidiLists (plugin.midi.getInputNames(), plugin.midi.getOutputNames());
    settings.setMidiSelection (plugin.prefs.midiInputName, plugin.prefs.midiOutputName,
                               plugin.prefs.midiControllerInputName);
    settings.setChannel (plugin.prefs.midiChannel);
    settings.setPacingMs (plugin.prefs.sysexPacingMs);
    settings.setMorphEmitMs (plugin.prefs.morphEmitMs);
    settings.setMorphReleaseGuardMs (plugin.prefs.morphReleaseGuardMs);
    settings.setMorphNoteSettleMs (plugin.prefs.morphNoteSettleMs);
    settings.setMorphStreamMode (plugin.prefs.morphStreamMode);
    settings.setMidiControllerThru (plugin.prefs.midiControllerThru);
    settings.setDarkTheme (plugin.prefs.darkTheme);
    settings.setGroupByBank (plugin.prefs.groupByBank);
    settings.setShowFileColumns (plugin.prefs.showFileColumns);
    settings.setHideDuplicates (plugin.prefs.hideDuplicates);
    settings.setTooltipsEnabled (plugin.prefs.showTooltips);
    settings.setAuditionNote (plugin.prefs.auditionNote);
    settings.setAuditionVelocity (plugin.prefs.auditionVelocity);
    settings.setAuditionDurationMs (plugin.prefs.auditionDurationMs);

    refreshLibraryView();
    updateModeButtons();
    // Tall enough for 32 device-buffer rows; user can resize further in supporting hosts.
    setResizable (true, true);
    // Minimum height is set by the Settings stack: below this the checklist and
    // Reset defaults fall off the bottom.
    setResizeLimits (960, 850, 2400, 1600);
    setSize (1100, 900);
    syncTooltipWindow();
    startTimerHz (4);
}

FmLibPlugAudioProcessorEditor::~FmLibPlugAudioProcessorEditor()
{
    plugin.releaseAllControllerNotes();
    plugin.library.clearListeners();
    plugin.midi.setStatusCallback (nullptr);
    plugin.midi.setControllerNoteOnCallback (nullptr);
    plugin.midi.setControllerNoteOffCallback (nullptr);
    setLookAndFeel (nullptr);
}

void FmLibPlugAudioProcessorEditor::syncTooltipWindow()
{
    // JUCE has no enable flag; a huge delay effectively disables tips.
    tooltipWindow.setMillisecondsBeforeTipAppears (plugin.prefs.showTooltips ? 700 : 0x7fffffff);
}

void FmLibPlugAudioProcessorEditor::syncChromeColours()
{
    const auto text = lnf.findColour (juce::Label::textColourId);
    const auto muted = fmlib::ThemePalette::forTheme (plugin.prefs.darkTheme).muted;
    title.setColour (juce::Label::textColourId, text);
    subtitle.setColour (juce::Label::textColourId, muted);
    versionLabel.setColour (juce::Label::textColourId, muted);
    status.setColour (juce::Label::textColourId, text);
    settings.applyThemeColours (plugin.prefs.darkTheme);
    sendLookAndFeelChange();
}

void FmLibPlugAudioProcessorEditor::setMidiStatus (const juce::String& s)
{
    lastMidiStatus = s;
    updateStatusBar();
}

void FmLibPlugAudioProcessorEditor::updateStatusBar()
{
    juce::String counts = juce::String (browserStats.totalInScope) + " total";
    if (browserStats.shown != browserStats.totalInScope)
        counts += " / " + juce::String (browserStats.shown) + " shown";
    if (! plugin.prefs.hideDuplicates && browserStats.duplicates > 0)
        counts += " / " + juce::String (browserStats.duplicates) + " dupes";

    if (lastMidiStatus.isNotEmpty())
        status.setText (counts + "  |  " + lastMidiStatus, juce::dontSendNotification);
    else
        status.setText (counts, juce::dontSendNotification);
}

void FmLibPlugAudioProcessorEditor::syncAuditionPrefsFromSettings()
{
    plugin.prefs.auditionNote = settings.getAuditionNote();
    plugin.prefs.auditionVelocity = settings.getAuditionVelocity();
    plugin.prefs.auditionDurationMs = settings.getAuditionDurationMs();
}

void FmLibPlugAudioProcessorEditor::syncMorphPrefsFromSettings()
{
    plugin.prefs.morphEmitMs = settings.getMorphEmitMs();
    plugin.prefs.morphReleaseGuardMs = settings.getMorphReleaseGuardMs();
    plugin.prefs.morphNoteSettleMs = settings.getMorphNoteSettleMs();
    plugin.prefs.midiControllerThru = settings.getMidiControllerThru();
    plugin.prefs.morphStreamMode = settings.getMorphStreamMode();
    morpher.setEmitIntervalMs (plugin.prefs.morphEmitMs);
    plugin.midi.setMorphReleaseGuardMs (plugin.prefs.morphReleaseGuardMs);
    plugin.midi.setControllerThru (plugin.prefs.midiControllerThru);
    plugin.midi.setMorphStreamMode (static_cast<fmlib::MorphStreamMode> (
        juce::jlimit (0, 1, plugin.prefs.morphStreamMode)));
    const int budget = juce::jlimit (14, 70,
                                     (plugin.prefs.morphEmitMs <= 50 ? 56
                                      : (plugin.prefs.morphEmitMs >= 200 ? 28 : 42)));
    plugin.midi.setMorphByteBudget (budget);
    plugin.persistPreferences();
}

int FmLibPlugAudioProcessorEditor::morphNoteLeadMs() const
{
    // Wire time is measured (busy queue plus any dump not scheduled yet); the settle
    // margin on top is how long the synth needs to apply the voice, which is the user's
    // to tune. Clamp the wire part only, so a large settle is never capped away.
    const int wire = plugin.midi.getMorphLeadEstimateMs() + juce::jmax (1, plugin.prefs.sysexPacingMs);
    const int settle = juce::jlimit (0, 100, plugin.prefs.morphNoteSettleMs);
    return juce::jlimit (settle, 400 + settle, wire + settle);
}

void FmLibPlugAudioProcessorEditor::prepareNextMorphJump()
{
    if (morpher.getNoteJumpMode() == fmlib::MorpherPanel::NoteJumpMode::off)
        return;
    if (! controllerHeldNotes.empty())
        return;
    morpher.applyNoteJump();
}

void FmLibPlugAudioProcessorEditor::syncMidiParamsFromSettings()
{
    const int prevChannel = plugin.midi.getChannel();
    plugin.midi.setChannel (settings.getChannel());
    plugin.midi.setSysexPacingMs (settings.getPacingMs());
    if (prevChannel != plugin.midi.getChannel())
        plugin.midi.invalidateMorphBaseline();
    plugin.persistPreferences();
}

void FmLibPlugAudioProcessorEditor::applyMidiPortsFromSettings()
{
    plugin.midi.openInputByName (settings.getMidiIn());
    plugin.midi.openControllerInputByName (settings.getMidiControllerIn());
    plugin.midi.openOutputByName (settings.getMidiOut());
    plugin.midi.invalidateMorphBaseline();
    plugin.persistPreferences();
    setMidiStatus ("MIDI ports opened");
}

void FmLibPlugAudioProcessorEditor::syncFoldersFromSettings()
{
    plugin.library.setBaseFolders (settings.getFolderPaths());
    plugin.persistPreferences();
    plugin.library.rescanAsync();
    refreshLibraryView();
    setMidiStatus ("Scanning...");
}

void FmLibPlugAudioProcessorEditor::syncTooltipsFromSettings()
{
    plugin.prefs.showTooltips = settings.getTooltipsEnabled();
    browser.setTooltipsEnabled (plugin.prefs.showTooltips);
    syncTooltipWindow();
    plugin.persistPreferences();
}

void FmLibPlugAudioProcessorEditor::restoreListFocus()
{
    if (lastListFocus == ListFocus::device && devicePanel.isVisible())
        devicePanel.getList().grabKeyboardFocus();
    else
        browser.getTable().grabKeyboardFocus();
}

void FmLibPlugAudioProcessorEditor::updateModeButtons()
{
    settingsBtn.setToggleState (uiMode == UiMode::settings, juce::dontSendNotification);
    morphBtn.setToggleState (uiMode == UiMode::morph, juce::dontSendNotification);
    morphBtn.setButtonText ("Morph");
}

void FmLibPlugAudioProcessorEditor::showLibraryMode()
{
    uiMode = UiMode::library;
    settings.setVisible (false);
    morpher.setVisible (false);
    browser.setVisible (true);
    devicePanel.setVisible (true);
    updateModeButtons();
    resized();
}

void FmLibPlugAudioProcessorEditor::showSettingsMode()
{
    uiMode = UiMode::settings;
    settings.setVisible (true);
    morpher.setVisible (false);
    browser.setVisible (false);
    devicePanel.setVisible (false);
    settings.applyThemeColours (plugin.prefs.darkTheme);
    updateModeButtons();
    resized();
}

void FmLibPlugAudioProcessorEditor::showMorphMode()
{
    uiMode = UiMode::morph;
    settings.setVisible (false);
    morpher.setVisible (true);
    browser.setVisible (true);
    devicePanel.setVisible (false);
    morpher.setMode (fmlib::MorpherPanel::Mode::morph);
    updateModeButtons();
    resized();
}

void FmLibPlugAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void FmLibPlugAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto header = r.removeFromTop (48);
    title.setBounds (header.removeFromLeft (140));
    subtitle.setBounds (header.removeFromLeft (260));
    versionLabel.setBounds (header.removeFromLeft (160));
    settingsBtn.setBounds (header.removeFromRight (90).reduced (2));
    morphBtn.setBounds (header.removeFromRight (80).reduced (2));
    auditionBtn.setBounds (header.removeFromRight (80).reduced (2));
    status.setBounds (r.removeFromTop (22));

    if (uiMode == UiMode::settings)
    {
        settings.setBounds (r);
    }
    else if (uiMode == UiMode::morph)
    {
        auto left = r.removeFromLeft (juce::jmax (320, r.getWidth() / 2));
        browser.setBounds (left);
        r.removeFromLeft (8);
        morpher.setBounds (r);
    }
    else
    {
        auto right = r.removeFromRight (320);
        devicePanel.setBounds (right);
        r.removeFromRight (8);
        browser.setBounds (r);
    }
}

void FmLibPlugAudioProcessorEditor::timerCallback()
{
    const int n = plugin.deviceBuffer.empty() ? 0 : static_cast<int> (plugin.deviceBuffer.getVoices().size());
    if (n != lastDeviceVoiceCount || plugin.deviceBuffer.isDirty())
    {
        lastDeviceVoiceCount = n;
        devicePanel.refreshList();
    }
    if (plugin.library.isScanning())
        setMidiStatus ("Scanning...");
}

void FmLibPlugAudioProcessorEditor::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    devicePanel.notifyDragEnded();
}

void FmLibPlugAudioProcessorEditor::refreshLibraryView()
{
    browser.setFavoritesOnly (plugin.prefs.favoritesOnly);
    auto entries = plugin.library.getEntriesCopy();
    browser.setEntries (std::move (entries), &plugin.favorites, &plugin.tags, &plugin.recent);
    browser.setBankFileView (plugin.prefs.bankFileView);
    browser.setGroupByBank (plugin.prefs.groupByBank);
    browser.setShowFileColumns (plugin.prefs.showFileColumns);
    browser.setHideDuplicates (plugin.prefs.hideDuplicates);
    browser.setTooltipsEnabled (plugin.prefs.showTooltips);
    const auto skipped = plugin.library.getSkippedFileCount();
    if (! plugin.library.isScanning())
    {
        if (skipped > 0)
            setMidiStatus ("Skipped files: " + juce::String (skipped));
        else if (lastMidiStatus.startsWithIgnoreCase ("Scanning"))
            setMidiStatus ({});
        else
            updateStatusBar();
    }
}

void FmLibPlugAudioProcessorEditor::sendDeviceBuffer()
{
    const auto& voices = plugin.deviceBuffer.getVoices();
    if (voices.empty())
    {
        setMidiStatus ("Device buffer is empty");
        return;
    }

    if (plugin.deviceBuffer.isFullBank())
    {
        if (auto bank = plugin.deviceBuffer.asBank())
        {
            if (plugin.midi.sendBank (*bank))
                setMidiStatus ("Sent device buffer (32 voices)");
        }
        return;
    }

    if (voices.size() == 1)
    {
        plugin.sendVoice (voices.front().voice);
        setMidiStatus ("Sent 1 voice from device buffer");
        return;
    }

    setMidiStatus ("Device buffer has " + juce::String ((int) voices.size())
                   + " voices - Send needs 1 voice or a full 32-voice bank");
}

void FmLibPlugAudioProcessorEditor::saveDeviceBuffer()
{
    if (plugin.deviceBuffer.empty())
    {
        auto* aw = new juce::AlertWindow ("Save", "Device buffer is empty.", juce::AlertWindow::NoIcon);
        fmlib::themeAlertWindow (*aw, lnf);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int)
        {
            std::unique_ptr<juce::AlertWindow> cleanup (aw);
            aw->setLookAndFeel (nullptr);
        }));
        return;
    }

    const auto folders = plugin.library.getBaseFolders();
    if (folders.empty())
    {
        auto* aw = new juce::AlertWindow ("Save", "Add a library folder in Settings first.", juce::AlertWindow::NoIcon);
        fmlib::themeAlertWindow (*aw, lnf);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([aw] (int)
        {
            std::unique_ptr<juce::AlertWindow> cleanup (aw);
            aw->setLookAndFeel (nullptr);
        }));
        return;
    }

    const juce::File startDir (juce::String::fromUTF8 (folders.front().string().c_str()));

    auto* aw = new juce::AlertWindow ("Save received presets", "File name (without .syx):", juce::AlertWindow::NoIcon);
    fmlib::themeAlertWindow (*aw, lnf);
    aw->addTextEditor ("name", "ReceivedBank", "Name");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, startDir] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (aw);
        aw->setLookAndFeel (nullptr);
        if (result != 1)
            return;

        const auto name = aw->getTextEditorContents ("name").toStdString();
        auto chooser = std::make_shared<juce::FileChooser> ("Choose destination folder", startDir);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, chooser, name] (const juce::FileChooser& fc)
                              {
                                  const auto destFile = fc.getResult();
                                  if (! destFile.isDirectory())
                                      return;

                                  const std::filesystem::path dest (destFile.getFullPathName().toStdString());
                                  fmlib::PatchWriter::WriteResult wr;
                                  constexpr bool overwrite = true;
                                  if (plugin.deviceBuffer.isFullBank())
                                  {
                                      auto bank = *plugin.deviceBuffer.asBank();
                                      wr = fmlib::PatchWriter::writeBank (dest, name, bank, plugin.midi.getChannel(), overwrite);
                                  }
                                  else
                                  {
                                      wr = fmlib::PatchWriter::writeSingleVoice (dest, name,
                                                                                plugin.deviceBuffer.getVoices().front().voice,
                                                                                plugin.midi.getChannel(), overwrite);
                                  }

                                  if (wr.ok)
                                  {
                                      plugin.deviceBuffer.markSaved();
                                      plugin.library.rescanAsync();
                                      setMidiStatus ("Saved " + juce::String::fromUTF8 (wr.path.string().c_str()));
                                  }
                                  else
                                  {
                                      setMidiStatus ("Save failed: " + juce::String::fromUTF8 (wr.error.c_str()));
                                  }
                              });
    }));
}
