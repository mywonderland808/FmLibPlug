#pragma once

#include "midi/MorphTransport.h"
#include "sysex/MorphLocks.h"
#include <juce_core/juce_core.h>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace fmlib
{

class AppPreferences
{
public:
    AppPreferences();

    /**
     * Member-initializer defaults only (does not read settings.xml).
     * Use for Reset defaults / double-click factory values.
     */
    static AppPreferences compiledDefaults();

    void load();
    void save() const;

    void loadFromFile (const juce::File& file);
    void saveToFile (const juce::File& file) const;

    std::vector<std::filesystem::path> baseFolders;
    juce::String midiInputName;
    juce::String midiOutputName;
    /** Optional second input for note/CC morph performance (separate from SysEx device). */
    juce::String midiControllerInputName;
    int midiChannel = 1;
    int sysexPacingMs = 20;
    /** Morph transport tick spacing hint (20-250); byte budget derived from this. */
    int morphEmitMs = 150;
    /**
     * How long morph SysEx waits after the last note-off (0-2000), so the backlog held
     * back by freqOnly does not re-latch the voice while the note is still releasing.
     * Raise it for patches with long release EGs; 0 disables the hold.
     */
    int morphReleaseGuardMs = 250;
    /**
     * Gap (0-100) between a morph voice dump finishing on the wire and the delayed
     * note-on of a note morph. Covers how long the synth needs to apply the voice;
     * the wire time itself is measured, so this is only the hardware settle margin.
     */
    int morphNoteSettleMs = 40;
    /** Default MorphLockGroup flags for new morph sessions. */
    uint32_t morphLockGroups = morphLockFactoryDefaults;
    float morphLockRefX = 0.0f;
    float morphLockRefY = 0.0f;
    /** Collapsible library tag filter panel. */
    bool tagFilterExpanded = false;
    bool morphLfoEnabled = false;
    /** Signed edge-LFO rate in Hz; negative = CCW, positive = CW. */
    float morphLfoRateHz = 0.25f;
    /**
     * On controller Note-On / Audition with morph jump:
     * 0 = off, 1 = random pad, 2 = cycle corners A-B-D-C.
     */
    int morphNoteJumpMode = 0;
    /** MorphStreamMode: 0 = all parameters, 1 = frequency only while notes sound. */
    int morphStreamMode = static_cast<int> (MorphStreamMode::freqOnly);
    /** Forward controller MIDI (notes/CC) to MIDI out. Default off. */
    bool midiControllerThru = false;
    bool darkTheme = true;
    /** Toolbar Bank vs Single: true = bank files (slot 1..32), false = single-voice files. */
    bool bankFileView = true;
    /** When bankFileView: show section headers grouped by bank file. */
    bool groupByBank = true;
    bool showFileColumns = false;
    bool hideDuplicates = false;
    bool showTooltips = true;
    /** Patch browser Favorites toggle (beside Tags filter). */
    bool favoritesOnly = false;
    int auditionNote = 60;
    int auditionVelocity = 100;
    int auditionDurationMs = 250;
    juce::StringArray favoriteIds;

private:
    explicit AppPreferences (bool loadFromDisk);
    juce::File getFile() const;
};

} // namespace fmlib
