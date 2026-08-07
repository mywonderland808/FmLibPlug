#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace fmlib
{

class SettingsPanel : public juce::Component
{
public:
    using ApplyFn = std::function<void()>;

    SettingsPanel();

    void setFolderPaths (const std::vector<std::filesystem::path>& paths);
    std::vector<std::filesystem::path> getFolderPaths() const;
    void addFolderPath (const juce::String& path);
    void removeSelectedFolder();

    void setMidiLists (const juce::StringArray& inputs, const juce::StringArray& outputs);
    juce::String getMidiIn() const { return midiIn.getText(); }
    juce::String getMidiOut() const { return midiOut.getText(); }
    void setMidiSelection (const juce::String& in, const juce::String& out);

    int getChannel() const { return (int) channel.getValue(); }
    void setChannel (int c) { channel.setValue (c); }
    int getPacingMs() const { return (int) pacing.getValue(); }
    void setPacingMs (int ms) { pacing.setValue (ms); }

    bool getDarkTheme() const { return dark.getToggleState(); }
    void setDarkTheme (bool d) { dark.setToggleState (d, juce::dontSendNotification); }
    bool getGroupByBank() const { return groupByBank.getToggleState(); }
    void setGroupByBank (bool v) { groupByBank.setToggleState (v, juce::dontSendNotification); }
    bool getShowFileColumns() const { return showFileColumns.getToggleState(); }
    void setShowFileColumns (bool v) { showFileColumns.setToggleState (v, juce::dontSendNotification); }
    bool getHideDuplicates() const { return hideDuplicates.getToggleState(); }
    void setHideDuplicates (bool v) { hideDuplicates.setToggleState (v, juce::dontSendNotification); }
    bool getTooltipsEnabled() const { return showTooltips.getToggleState(); }
    void setTooltipsEnabled (bool v) { showTooltips.setToggleState (v, juce::dontSendNotification); }

    int getAuditionNote() const { return (int) auditionNote.getValue(); }
    void setAuditionNote (int n) { auditionNote.setValue (n); }
    int getAuditionVelocity() const { return (int) auditionVel.getValue(); }
    void setAuditionVelocity (int v) { auditionVel.setValue (v); }
    int getAuditionDurationMs() const { return (int) auditionMs.getValue(); }
    void setAuditionDurationMs (int ms) { auditionMs.setValue (ms); }

    void resized() override;
    void lookAndFeelChanged() override;
    /** Apply label/toggle colours from the active theme (does not rely on findColour). */
    void applyThemeColours (bool darkTheme);

    ApplyFn onApply;
    ApplyFn onRefreshMidi;
    ApplyFn onAddFolder;
    ApplyFn onRemoveFolder;
    ApplyFn onAutoTag;
    ApplyFn onResetTags;
    ApplyFn onRescan;
    ApplyFn onThemeChanged;
    /** Live update for browser-related toggles (file columns, hide dupes, group-by-bank). */
    ApplyFn onBrowserPrefsChanged;

private:
    void layoutLabeled (juce::Rectangle<int>& area, juce::Label& label, juce::Component& control, int controlW = 0);

    juce::Label foldersLabel { {}, "Library folders" };
    juce::ListBox folderList { "folders", nullptr };
    juce::TextButton addFolder { "Add folder" }, removeFolder { "Remove" }, apply { "Apply" };
    juce::TextButton refreshMidi { "Refresh MIDI" }, rescan { "Rescan library" };
    juce::TextButton autoTag { "Auto-tag library" }, resetTags { "Reset all tags" };

    juce::Label midiInLabel { {}, "MIDI input" };
    juce::Label midiOutLabel { {}, "MIDI output" };
    juce::Label channelLabel { {}, "MIDI channel" };
    juce::Label pacingLabel { {}, "SysEx pacing (ms)" };
    juce::Label noteLabel { {}, "Audition note" };
    juce::Label velLabel { {}, "Audition velocity" };
    juce::Label durLabel { {}, "Audition duration (ms)" };

    juce::ComboBox midiIn, midiOut;
    juce::Slider channel, pacing, auditionNote, auditionVel, auditionMs;

    juce::ToggleButton dark { "Dark theme" };
    juce::ToggleButton groupByBank { "Group bank voices by file headers" };
    juce::ToggleButton showFileColumns { "Show file / folder columns" };
    juce::ToggleButton hideDuplicates { "Hide duplicate voices" };
    juce::ToggleButton showTooltips { "Show tooltips" };

    juce::Label checklist {
        {},
        "Hardware checklist: Memory Protect OFF; MIDI channel match; "
        "Computer OUT -> synth IN; synth OUT -> Computer IN; prefer a proper interface over cheap USB-MIDI cables for SysEx."
    };

    class FolderModel : public juce::ListBoxModel
    {
    public:
        SettingsPanel* owner = nullptr;
        int getNumRows() override { return owner != nullptr ? static_cast<int> (owner->folders.size()) : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    } folderModel;

    std::vector<juce::String> folders;
    friend class FolderModel;
};

} // namespace fmlib
