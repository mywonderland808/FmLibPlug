#pragma once

#include "PluginProcessor.h"
#include "ui/DeviceBufferPanel.h"
#include "ui/LookAndFeel_FmLibPlug.h"
#include "ui/MorpherPanel.h"
#include "ui/PatchBrowser.h"
#include "ui/SettingsPanel.h"

class FmLibPlugAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::DragAndDropContainer,
                                      private juce::Timer
{
public:
    explicit FmLibPlugAudioProcessorEditor (FmLibPlugAudioProcessor&);
    ~FmLibPlugAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshLibraryView();
    void applySettingsFromPanel();
    void saveDeviceBuffer();
    void sendDeviceBuffer();
    void syncChromeColours();
    void updateModeButtons();
    void showLibraryMode();
    void showSettingsMode();
    void showMorphMode();
    void restoreListFocus();
    void syncAuditionPrefsFromSettings();
    void updateStatusBar();
    void setMidiStatus (const juce::String& s);

    FmLibPlugAudioProcessor& plugin;
    fmlib::LookAndFeel_FmLibPlug lnf;
    juce::Label title { {}, "FmLibPlug" };
    juce::Label subtitle { {}, "FM SysEx librarian for DX7 / TX7" };
    juce::Label versionLabel;
    juce::TextButton settingsBtn { "Settings" };
    juce::TextButton auditionBtn { "Audition" };
    juce::TextButton morphBtn { "Morph" };
    juce::Label status;
    fmlib::PatchBrowser browser;
    fmlib::DeviceBufferPanel devicePanel;
    fmlib::SettingsPanel settings;
    fmlib::MorpherPanel morpher;
    enum class UiMode { library, settings, morph };
    UiMode uiMode = UiMode::library;
    enum class ListFocus { browser, device };
    ListFocus lastListFocus = ListFocus::browser;
    int lastDeviceVoiceCount = -1;
    juce::String lastMidiStatus;
    fmlib::BrowserStats browserStats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmLibPlugAudioProcessorEditor)
};
