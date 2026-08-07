#include "PluginEditor.h"
#include "BuildId.h"
#include "library/PatchWriter.h"
#include "sysex/SysexParser.h"

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

    plugin.library.addListener ([safe = juce::Component::SafePointer<FmLibPlugAudioProcessorEditor> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->refreshLibraryView();
        });
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
    browser.setBankFileViewChanged ([this] (bool banks)
    {
        plugin.prefs.bankFileView = banks;
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
        plugin.auditionNote();
        restoreListFocus();
    };

    morpher.onMorph = [this] (const fmlib::VoiceData& v) { plugin.sendVoice (v); };
    morpher.onPresetsChanged = [this] { plugin.persistPreferences(); };
    morpher.onRequestAssignCorner = [this] (int corner)
    {
        if (auto sel = browser.getSelectedVoice())
        {
            morpher.setCorner (corner, sel->voice, juce::String::fromUTF8 (sel->voiceName.c_str()));
            setMidiStatus ("Assigned corner " + juce::String::charToString (static_cast<juce::juce_wchar> ('A' + corner))
                                + " <- " + juce::String::fromUTF8 (sel->voiceName.c_str()));
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
    settings.onApply = [this] { applySettingsFromPanel(); };
    settings.onAutoTag = [this]
    {
        plugin.autoTagLibrary();
        refreshLibraryView();
        setMidiStatus ("Auto-tagged library (see Tags column; click Tags to edit)");
    };
    settings.onResetTags = [this]
    {
        auto* aw = new juce::AlertWindow ("Reset all tags",
                                          "Delete every tag for every voice? This cannot be undone.",
                                          juce::AlertWindow::WarningIcon);
        aw->setLookAndFeel (&lnf);
        aw->setColour (juce::AlertWindow::backgroundColourId, lnf.findColour (juce::AlertWindow::backgroundColourId));
        aw->setColour (juce::AlertWindow::textColourId, lnf.findColour (juce::AlertWindow::textColourId));
        aw->setColour (juce::AlertWindow::outlineColourId, lnf.findColour (juce::AlertWindow::outlineColourId));
        aw->sendLookAndFeelChange();
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
    settings.setMidiSelection (plugin.prefs.midiInputName, plugin.prefs.midiOutputName);
    settings.setChannel (plugin.prefs.midiChannel);
    settings.setPacingMs (plugin.prefs.sysexPacingMs);
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
    setResizeLimits (960, 780, 2400, 1600);
    setSize (1100, 900);
    startTimerHz (4);
}

FmLibPlugAudioProcessorEditor::~FmLibPlugAudioProcessorEditor()
{
    plugin.library.clearListeners();
    plugin.midi.setStatusCallback (nullptr);
    setLookAndFeel (nullptr);
}

void FmLibPlugAudioProcessorEditor::syncChromeColours()
{
    const auto text = lnf.findColour (juce::Label::textColourId);
    const auto muted = plugin.prefs.darkTheme ? juce::Colour (0xffa8b0bb) : juce::Colour (0xff3c4043);
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

void FmLibPlugAudioProcessorEditor::refreshLibraryView()
{
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

void FmLibPlugAudioProcessorEditor::applySettingsFromPanel()
{
    plugin.prefs.darkTheme = settings.getDarkTheme();
    plugin.prefs.groupByBank = settings.getGroupByBank();
    plugin.prefs.showFileColumns = settings.getShowFileColumns();
    plugin.prefs.hideDuplicates = settings.getHideDuplicates();
    plugin.prefs.showTooltips = settings.getTooltipsEnabled();
    plugin.prefs.auditionNote = settings.getAuditionNote();
    plugin.prefs.auditionVelocity = settings.getAuditionVelocity();
    plugin.prefs.auditionDurationMs = settings.getAuditionDurationMs();
    lnf.setDark (plugin.prefs.darkTheme);
    syncChromeColours();
    sendLookAndFeelChange();
    plugin.library.setBaseFolders (settings.getFolderPaths());
    plugin.midi.setChannel (settings.getChannel());
    plugin.midi.setSysexPacingMs (settings.getPacingMs());
    plugin.midi.openInputByName (settings.getMidiIn());
    plugin.midi.openOutputByName (settings.getMidiOut());
    plugin.persistPreferences();
    plugin.library.rescanAsync();
    refreshLibraryView();
    showLibraryMode();
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
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, "Save", "Device buffer is empty.");
        return;
    }

    const auto folders = plugin.library.getBaseFolders();
    if (folders.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Save", "Add a library folder in Settings first.");
        return;
    }

    auto* aw = new juce::AlertWindow ("Save received presets", "File name (without .syx):", juce::AlertWindow::QuestionIcon);
    aw->setLookAndFeel (&lnf);
    aw->sendLookAndFeelChange();
    aw->addTextEditor ("name", "ReceivedBank", "Name");
    aw->addComboBox ("folder", {}, "Destination folder");
    auto* box = aw->getComboBoxComponent ("folder");
    for (const auto& f : folders)
        box->addItem (juce::String::fromUTF8 (f.string().c_str()), box->getNumItems() + 1);
    box->setSelectedItemIndex (0);
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (aw);
        aw->setLookAndFeel (nullptr);
        if (result != 1)
            return;
        const auto name = aw->getTextEditorContents ("name").toStdString();
        const int idx = aw->getComboBoxComponent ("folder")->getSelectedItemIndex();
        const auto foldersNow = plugin.library.getBaseFolders();
        if (! juce::isPositiveAndBelow (idx, static_cast<int> (foldersNow.size())))
            return;
        const auto dest = foldersNow[static_cast<size_t> (idx)];

        fmlib::PatchWriter::WriteResult wr;
        const bool overwrite = true;
        if (plugin.deviceBuffer.isFullBank())
        {
            auto bank = *plugin.deviceBuffer.asBank();
            wr = fmlib::PatchWriter::writeBank (dest, name, bank, plugin.midi.getChannel(), overwrite);
        }
        else
        {
            wr = fmlib::PatchWriter::writeSingleVoice (dest, name, plugin.deviceBuffer.getVoices().front().voice,
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
    }));
}
