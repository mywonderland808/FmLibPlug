#pragma once

#include "library/MorphPresetStore.h"
#include "sysex/Dx7Formats.h"
#include "sysex/MorphLocks.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace fmlib
{

class MorpherPanel : public juce::Component,
                     private juce::ListBoxModel,
                     private juce::Timer
{
public:
    /** dragEmit: coalesced param stream. liveAllParams: stream locked-group changes even in freqOnly. */
    using MorphFn = std::function<void(const VoiceData& voice, bool dragEmit, bool liveAllParams)>;
    using PersistFn = std::function<void()>;
    using SaveToDeviceSlotFn = std::function<void(const VoiceData& voice, int slot1to32)>;

    MorpherPanel();
    ~MorpherPanel() override;

    enum class Mode { morph, presets };

    void setMode (Mode m);

    void setCorner (int corner0to3, const VoiceData& v, const juce::String& name);
    /** Corner voices only; locks and lock reference are untouched (see resetLocksToDefaults). */
    void clearCorners();
    void setMarkerPosition (float x, float y);
    void jumpToRandomPosition();
    void jumpToNextEdge();
    void setPresetStore (MorphPresetStore* store);
    MorphPreset currentAsPreset (const juce::String& name) const;
    bool allCornersReady() const;

    void setEmitIntervalMs (int ms);

    void setLockGroups (uint32_t groups);
    /** Session defaults snapshotted at editor open (Reset restores these). */
    void setDefaultLockGroups (uint32_t groups);
    uint32_t getDefaultLockGroups() const { return defaultLockGroups; }
    void setDefaultLockRefPosition (float x, float y);
    float getDefaultLockRefX() const { return defaultLockRefX; }
    float getDefaultLockRefY() const { return defaultLockRefY; }
    /** Save current locks + lock-ref as session/prefs defaults (Reset uses these). */
    void saveLocksAsDefaults();
    /** Restore lock groups and lock-ref to the saved defaults. */
    void resetLocksToDefaults();

    void setLockRefPosition (float x, float y);

    /** When true, LFO/pad morph emits are suppressed (paused after library click on Morph page). */
    void setEgressPaused (bool paused);
    bool isEgressPaused() const { return egressPaused; }
    /** Clear pause and send the morph at the current pad position. */
    void reemitCurrent();

    /** Turn off Edge LFO and Note morph (mutually exclusive drivers stay off). */
    void stopMotionDrivers();

    void setLfoEnabled (bool on);
    bool getLfoEnabled() const { return lfoEnabled; }
    /** Signed Hz; negative = CCW, positive = CW. Near-zero disables motion while toggle stays on. */
    void setLfoRateHz (float hz);
    float getLfoRateHz() const { return lfoRateHz; }

    enum class NoteJumpMode { off = 0, random = 1, edges = 2 };
    void setNoteJumpMode (NoteJumpMode m);
    NoteJumpMode getNoteJumpMode() const { return noteJumpMode; }
    /** Apply Off/Random/Edges jump; returns true if the pad moved (and morph was emitted). */
    bool applyNoteJump();

    void resized() override;
    void paint (juce::Graphics& g) override;
    void lookAndFeelChanged() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    MorphFn onMorph;
    PersistFn onPresetsChanged;
    PersistFn onMorphUiPrefsChanged;
    SaveToDeviceSlotFn onSaveToDeviceSlot;
    std::function<void()> onRevealMorphPresetsFile;
    std::function<void()> onMorphInvalidate;
    /** Fired when Note morph Random/Edges becomes active — optional idle pad jump. */
    std::function<void()> onNoteJumpArmed;
    /** Fired after a morph preset is applied from the list (session live again). */
    std::function<void()> onPresetApplied;
    /**
     * Fired when morph should leave a library-audition pause: pad click/drag start,
     * or turning Edge LFO / Note morph back on. Not fired on ordinary LFO ticks.
     */
    std::function<void()> onPadGestureStarted;
    std::function<void(int)> onRequestAssignCorner;
    std::function<void(const juce::String&)> onStatus;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    void updatePositionFromPoint (juce::Point<float> p);
    void tryEmitDrag();
    void emitMorph (bool dragEmit, bool liveAllParams = false);
    bool padContains (juce::Point<float> p) const;
    void cancelPadGesture();
    /** Unpause after a Morph-page library click (same path as using the pad). */
    void resumeEgressIfPaused();
    void updateLockRefFromPoint (juce::Point<float> p);
    void commitLockRef();
    void drawPadCrosshair (juce::Graphics& g, float x, float y, juce::Colour colour) const;
    void drawPadCoordLabel (juce::Graphics& g, float x, float y, const juce::String& text,
                            juce::Colour colour) const;
    void refreshPresetList();
    void saveCurrentPreset();
    void loadSelectedPreset (bool switchToMorphPad);
    void deleteSelectedPreset();
    void saveSelectedPresetToDeviceSlot();
    void showPresetContextMenu (int row);
    void updateHint();
    juce::String cornerDisplayName (int corner) const;
    static int percent99 (float t);
    VoiceData morphAt (float x, float y) const;
    VoiceData morphAtCurrent() const;
    void syncLockChipsFromFlags();
    void applyLockChipToggles();
    /** Flow-lays the lock chips over as many rows as the width needs (no clipped names).
     *  Stops placing chips once fewer than minLeaveBelow pixels remain for chrome below. */
    void layoutLockChips (juce::Rectangle<int>& area, int minLeaveBelow);
    void syncLfoUi();
    void syncNoteJumpUi();
    void ensureTimerRunning();
    void advanceLfo (double deltaSeconds);
    void updateMorphControlsEnabled();
    int effectiveEmitIntervalMs() const;
    static void edgePosition (float phase01, bool clockwise, float& x, float& y);

    Mode mode = Mode::morph;
    VoiceData corners[4] {};
    juce::String cornerNames[4];
    bool cornerSet[4] { false, false, false, false };
    float posX = 0.0f, posY = 0.0f;
    float lockRefX = 0.0f, lockRefY = 0.0f;
    float defaultLockRefX = 0.0f, defaultLockRefY = 0.0f;
    uint32_t lastMorphEmitMs = 0;
    int emitIntervalMs = 150;
    bool padDragging = false;
    bool lockRefDragging = false;
    bool padGesture = false;
    bool pendingMorph = false;
    bool suppressPresetAutoLoad = false;
    juce::Point<float> padDownPos;
    std::optional<VoiceData> lastSentVoice;

    uint32_t lockGroups = morphLockFactoryDefaults;
    uint32_t defaultLockGroups = morphLockFactoryDefaults;
    bool lfoEnabled = false;
    bool egressPaused = false;
    float lfoRateHz = 0.25f;
    float lfoPhase = 0.0f;
    int lastLfoStep = -1;
    uint32_t lastTimerMs = 0;
    NoteJumpMode noteJumpMode = NoteJumpMode::off;
    int edgeJumpIndex = 0;

    juce::TextButton modeMorph { "Morph" }, modePresets { "Presets" };
    juce::TextButton assignA { "Set A" }, assignB { "Set B" }, assignC { "Set C" }, assignD { "Set D" };
    juce::TextButton clearBtn { "Clear" };
    juce::Label hint { {}, "Select a library voice, Set A-D, then drag the pad." };
    juce::Label locksLabel { {}, "Locks" };
    juce::ToggleButton lockEg { "EG" }, lockLevels { "Levels" }, lockFreqCoarse { "Coarse" };
    juce::ToggleButton lockAlgo { "Algorithm" }, lockFeedback { "Feedback" }, lockSync { "Key sync" };
    juce::ToggleButton lockPitchEg { "Pitch EG" }, lockLfo { "LFO" };
    juce::ToggleButton lockScaling { "Scaling" }, lockSens { "AMS" }, lockTranspose { "Transpose" };
    juce::ToggleButton lockFreqFine { "Fine" };
    juce::TextButton resetLocksBtn { "Reset" }, setDefaultLocksBtn { "Set default" };
    juce::ToggleButton lfoToggle { "Edge LFO" };
    juce::Slider lfoRate;
    juce::Label lfoRateLabel { {}, "Rate" };
    juce::Label noteJumpLabel { {}, "Note morph" };
    juce::ToggleButton noteJumpOff { "Off" }, noteJumpRandom { "Random" }, noteJumpEdges { "Edges" };
    juce::Rectangle<int> padBounds;

    juce::ListBox presetList { "morphPresets", this };
    juce::TextEditor presetName;
    juce::TextButton savePreset { "Save" }, deletePreset { "Delete" };
    juce::Label deviceSlotLabel { {}, "Device buffer slot" };
    juce::ComboBox deviceSlot;
    juce::TextButton saveToDevice { "Save preset to slot" };
    MorphPresetStore* store = nullptr;
};

} // namespace fmlib
