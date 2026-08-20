#pragma once

#include "PluginProcessor.h"
#include "ui/DeviceBufferPanel.h"
#include "ui/GlobalsPanel.h"
#include "ui/LookAndFeel_FmLibPlug.h"
#include "ui/MorpherPanel.h"
#include "ui/PatchBrowser.h"
#include "ui/SettingsPanel.h"
#include "ui/TxSystemPanel.h"
#include "util/AuditionHoldInputs.h"

class FmLibPlugAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::DragAndDropContainer,
                                      private juce::Timer,
                                      private juce::KeyListener
{
public:
    explicit FmLibPlugAudioProcessorEditor (FmLibPlugAudioProcessor&);
    ~FmLibPlugAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override;
    bool keyStateChanged (bool isKeyDown, juce::Component*) override;
    void timerCallback() override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;
    void refreshLibraryView();
    void saveDeviceBuffer();
    void sendDeviceBuffer();
    void syncChromeColours();
    void updateModeButtons();
    void showLibraryMode();
    void showSettingsMode();
    void showMorphMode();
    void showGlobalsMode();
    void restoreListFocus();
    void syncMorphPanelFromProcessor();
    void syncAuditionPrefsFromSettings();
    void syncMorphPrefsFromSettings (bool persist);
    int morphNoteLeadMs() const;
    void prepareNextMorphJump();
    void syncMidiParamsFromSettings (bool persist);
    void applyMidiPortsFromSettings();
    void syncFoldersFromSettings();
    void syncTooltipsFromSettings();
    void updateStatusBar();
    void setMidiStatus (const juce::String& s);
    void syncTooltipWindow();
    void endMorphPerformance();
    void pauseMorphPerformance();
    void resumeMorphPerformance();
    void handleLibraryVoiceLoad (const fmlib::PatchEntry& e, bool loadBank);
    void handleMenuAudition (const fmlib::PatchEntry& e);
    void assignMorphCorner (int corner0to3, const fmlib::PatchEntry& e);
    void startAuditionHold();
    void stopAuditionHold();
    void applyAuditionHoldAction (fmlib::AuditionHoldInputs::Action action);
    void syncAuditionButtonVisual();
    int previewNoteLeadMs() const;

    FmLibPlugAudioProcessor& plugin;
    fmlib::LookAndFeel_FmLibPlug lnf;
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::Label title { {}, "FmLibPlug" };
    juce::Label subtitle { {}, "FM SysEx librarian for DX7 / TX7" };
    juce::Label versionLabel;
    juce::TextButton settingsBtn { "Settings" };
    juce::TextButton auditionBtn { "Audition" };
    juce::TextButton morphBtn { "Morph" };
    juce::TextButton globalsBtn { "Globals" };
    juce::Label status;
    fmlib::PatchBrowser browser;
    fmlib::DeviceBufferPanel devicePanel;
    fmlib::GlobalsPanel globalsPanel;
    fmlib::SettingsPanel settings;
    fmlib::MorpherPanel morpher;
    fmlib::TxSystemPanel txSystem;
    enum class UiMode { library, settings, morph, globals };
    UiMode uiMode = UiMode::library;
    enum class ListFocus { browser, device };
    ListFocus lastListFocus = ListFocus::browser;
    int lastDeviceVoiceCount = -1;
    juce::String lastMidiStatus;
    fmlib::BrowserStats browserStats;
    fmlib::AuditionHoldInputs auditionHoldInputs;
    bool ignoreAuditionButtonState = false;
    bool morphPaused = false;
    int lastSyncedMotion = -1;
    uint32_t lastSyncedLockGroups = 0xffffffffu;
    bool lastSyncedCornersReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmLibPlugAudioProcessorEditor)
};
