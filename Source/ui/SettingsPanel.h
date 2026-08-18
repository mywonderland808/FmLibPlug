#pragma once

#include "prefs/AppPreferences.h"
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
    void setLibraryFolders (const std::vector<LibraryFolder>& folders);
    std::vector<std::filesystem::path> getFolderPaths() const;
    std::vector<LibraryFolder> getLibraryFolders() const;
    void addFolderPath (const juce::String& path);
    void removeSelectedFolder();
    void toggleFolderEnabled (int row);

    void setMidiLists (const juce::StringArray& inputs, const juce::StringArray& outputs);
    juce::String getMidiIn() const { return midiIn.getText(); }
    juce::String getMidiOut() const { return midiOut.getText(); }
    juce::String getMidiControllerIn() const;
    void setMidiSelection (const juce::String& in, const juce::String& out, const juce::String& controllerIn = {});

    int getChannel() const { return (int) channel.getValue(); }
    void setChannel (int c) { channel.setValue (c, juce::dontSendNotification); }
    int getPacingMs() const { return (int) pacing.getValue(); }
    void setPacingMs (int ms) { pacing.setValue (ms, juce::dontSendNotification); }
    int getMorphEmitMs() const { return (int) morphEmit.getValue(); }
    void setMorphEmitMs (int ms) { morphEmit.setValue (ms, juce::dontSendNotification); }
    int getMorphReleaseGuardMs() const { return (int) morphRelease.getValue(); }
    void setMorphReleaseGuardMs (int ms) { morphRelease.setValue (ms, juce::dontSendNotification); }
    int getMorphNoteSettleMs() const { return (int) noteSettle.getValue(); }
    void setMorphNoteSettleMs (int ms) { noteSettle.setValue (ms, juce::dontSendNotification); }
    int getMorphStreamMode() const { return morphStream.getSelectedItemIndex(); }
    void setMorphStreamMode (int index)
    {
        morphStream.setSelectedItemIndex (juce::jlimit (0, 1, index), juce::dontSendNotification);
    }

    bool getMidiControllerThru() const { return midiControllerThru.getToggleState(); }
    void setMidiControllerThru (bool v) { midiControllerThru.setToggleState (v, juce::dontSendNotification); }

    bool getDarkTheme() const { return dark.getToggleState(); }
    void setDarkTheme (bool d) { dark.setToggleState (d, juce::dontSendNotification); }
    bool getShowFileColumns() const { return showFileColumns.getToggleState(); }
    void setShowFileColumns (bool v) { showFileColumns.setToggleState (v, juce::dontSendNotification); }
    bool getHideDuplicates() const { return hideDuplicates.getToggleState(); }
    void setHideDuplicates (bool v) { hideDuplicates.setToggleState (v, juce::dontSendNotification); }
    bool getTooltipsEnabled() const { return showTooltips.getToggleState(); }
    void setTooltipsEnabled (bool v) { showTooltips.setToggleState (v, juce::dontSendNotification); }

    int getAuditionNote() const { return (int) auditionNote.getValue(); }
    void setAuditionNote (int n) { auditionNote.setValue (n, juce::dontSendNotification); }
    int getAuditionVelocity() const { return (int) auditionVel.getValue(); }
    void setAuditionVelocity (int v) { auditionVel.setValue (v, juce::dontSendNotification); }
    int getAuditionDurationMs() const { return (int) auditionMs.getValue(); }
    void setAuditionDurationMs (int ms) { auditionMs.setValue (ms, juce::dontSendNotification); }

    /** Restore factory values for morph / audition / UI prefs, pacing and controller thru
        (not MIDI ports/channel, tags, or folders). */
    void resetNonMidiDefaults();

    void resized() override;
    void lookAndFeelChanged() override;
    /** Apply label/toggle colours from the active theme (does not rely on findColour). */
    void applyThemeColours (bool darkTheme);

    ApplyFn onRefreshMidi;
    ApplyFn onAddFolder;
    ApplyFn onRemoveFolder;
    ApplyFn onAutoTag;
    ApplyFn onResetTags;
    ApplyFn onRescan;
    ApplyFn onThemeChanged;
    ApplyFn onBrowserPrefsChanged;
    ApplyFn onMorphPrefsChanged;
    /** Persist morph prefs after a slider drag ends. */
    ApplyFn onMorphPrefsPersist;
    /** Channel / pacing only (does not reopen ports). */
    ApplyFn onMidiParamsChanged;
    /** Persist MIDI params after a slider drag ends. */
    ApplyFn onMidiParamsPersist;
    /** Apply MIDI device in/out and controller in from the current combo selections. */
    ApplyFn onApplyMidiPorts;
    ApplyFn onFoldersChanged;
    ApplyFn onAuditionChanged;
    /** Persist audition prefs after a slider drag ends. */
    ApplyFn onAuditionPersist;
    ApplyFn onTooltipsChanged;

private:
    void layoutLabeled (juce::Rectangle<int>& area, juce::Label& label, juce::Component& control, int controlW = 0);
    void wireSliderFactoryDoubleClick();

    juce::Label foldersLabel { {}, "Library folders" };
    juce::ListBox folderList { "folders", nullptr };
    juce::TextButton addFolder { "Add folder" }, removeFolder { "Remove" };
    juce::TextButton refreshMidi { "Refresh MIDI" }, rescan { "Rescan library" };
    juce::TextButton applyMidiPorts { "Apply MIDI settings" };
    juce::TextButton autoTag { "Auto-tag library" }, resetTags { "Reset all tags" };
    juce::TextButton resetDefaults { "Reset defaults" };

    juce::Label midiInLabel { {}, "MIDI input (device)" };
    juce::Label midiControllerLabel { {}, "MIDI controller in" };
    juce::Label midiOutLabel { {}, "MIDI output" };
    juce::Label channelLabel { {}, "MIDI channel" };
    juce::Label pacingLabel { {}, "SysEx pacing (ms)" };
    juce::Label morphEmitLabel { {}, "Morph wire budget (ms)" };
    juce::Label morphReleaseLabel { {}, "Morph release hold (ms)" };
    juce::Label noteSettleLabel { {}, "Note morph settle (ms)" };
    juce::Label morphStreamLabel { {}, "Morph while playing" };
    juce::Label noteLabel { {}, "Audition note" };
    juce::Label velLabel { {}, "Audition velocity" };
    juce::Label durLabel { {}, "Audition duration (ms)" };

    juce::ComboBox midiIn, midiControllerIn, midiOut, morphStream;
    juce::Slider channel, pacing, morphEmit, morphRelease, noteSettle, auditionNote, auditionVel, auditionMs;

    juce::ToggleButton dark { "Dark theme" };
    juce::ToggleButton showFileColumns { "Show file / folder columns" };
    juce::ToggleButton hideDuplicates { "Hide duplicate voices" };
    juce::ToggleButton showTooltips { "Show tooltips" };
    juce::ToggleButton midiControllerThru { "Forward controller MIDI to output" };

    juce::Label checklist {
        {},
        "Hardware checklist: Memory Protect OFF; MIDI channel match; "
        "Computer OUT -> synth IN; synth OUT -> Computer IN (device input); "
        "optional controller keyboard -> MIDI controller in (TX7 local keys do not reach the plugin). "
        "Enable Forward controller MIDI to send controller/DAW notes to the synth. "
        "Morph while playing: Frequency only glides pitch and saves the rest for the next silence; "
        "All parameters sweeps everything live, which clicks on a DX7 mkI / TX7 (often musically). "
        "Morph release hold keeps that saved-up dump out of the release tail of the last note; "
        "dragging the pad yourself cancels that wait. "
        "Note morph settle is the gap the synth gets to apply a voice before the delayed note-on. "
        "Reset defaults restores morph, audition, UI options, SysEx pacing and controller thru "
        "(not MIDI ports/channel, tags, or library folders). "
        "Prefer a proper interface over cheap USB-MIDI cables for SysEx."
    };

    class FolderModel : public juce::ListBoxModel
    {
    public:
        SettingsPanel* owner = nullptr;
        int getNumRows() override { return owner != nullptr ? static_cast<int> (owner->folders.size()) : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    } folderModel;

    std::vector<LibraryFolder> folders;
    friend class FolderModel;
};

} // namespace fmlib
