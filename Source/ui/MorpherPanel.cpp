#include "ui/MorpherPanel.h"
#include "host/MorphHostState.h"
#include "sysex/VoiceMorpher.h"
#include "ui/ThemePalette.h"
#include <cmath>

namespace fmlib
{

namespace
{
constexpr float kPadDragThresholdPx = 4.0f;
constexpr int kLfoTimerMs = 16;

/** Width a toggle needs for its full label, using the tick-box metrics of LookAndFeel_V4. */
int toggleWidthFor (const juce::ToggleButton& b, int height)
{
    const float fontSize = juce::jmin (15.0f, (float) height * 0.75f);
    const juce::Font font { juce::FontOptions (fontSize) };
    return juce::GlyphArrangement::getStringWidthInt (font, b.getButtonText())
         + juce::roundToInt (fontSize * 1.1f) + 16;
}

float rateWithClockwise (float hz, bool clockwise)
{
    float mag = std::abs (hz);
    if (mag < kMorphLfoPauseHz)
        mag = 0.25f;
    return clockwise ? mag : -mag;
}
} // namespace

MorpherPanel::MorpherPanel()
{
    modeMorph.setClickingTogglesState (true);
    modePresets.setClickingTogglesState (true);
    modeMorph.setToggleState (true, juce::dontSendNotification);
    modeMorph.onClick = [this] { setMode (Mode::morph); };
    modePresets.onClick = [this] { setMode (Mode::presets); };
    addAndMakeVisible (modeMorph);
    addAndMakeVisible (modePresets);

    auto wireAssign = [this] (juce::TextButton& b, int corner)
    {
        b.onClick = [this, corner]
        {
            if (onRequestAssignCorner)
                onRequestAssignCorner (corner);
        };
        addAndMakeVisible (b);
    };
    wireAssign (assignA, 0);
    wireAssign (assignB, 1);
    wireAssign (assignC, 2);
    wireAssign (assignD, 3);
    clearBtn.setTooltip ("Clear the four corner voices (A-D). Locks and lock reference stay; use Reset for those.");
    clearBtn.onClick = [this] { clearCorners(); };
    addAndMakeVisible (clearBtn);

    hint.setJustificationType (juce::Justification::centredLeft);
    hint.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (hint);

    locksLabel.setJustificationType (juce::Justification::centredLeft);
    locksLabel.setFont (juce::FontOptions (12.0f));
    locksLabel.setTooltip (
        "Locked groups are taken from the hollow lock-reference marker (right-drag on the pad), "
        "not from the main morph marker. Moving Ref changes those parameters; MIDI is sent on the "
        "next pad click/drag even if the main marker did not move.");
    addAndMakeVisible (locksLabel);

    auto wireLock = [this] (juce::ToggleButton& b, const juce::String& tip)
    {
        b.setClickingTogglesState (true);
        b.setTooltip (tip);
        b.onClick = [this] { applyLockChipToggles(); };
        addAndMakeVisible (b);
    };
    wireLock (lockEg, "Lock operator EG rates/levels to the lock-reference point");
    wireLock (lockLevels, "Lock operator output levels to the lock-reference point");
    wireLock (lockFreqCoarse, "Lock osc mode + coarse frequency to the lock-reference point");
    wireLock (lockFreqFine, "Lock fine frequency + detune to the lock-reference point");
    wireLock (lockAlgo, "Lock algorithm to the lock-reference point");
    wireLock (lockFeedback, "Lock feedback to the lock-reference point");
    wireLock (lockSync, "Lock oscillator key sync to the lock-reference point");
    wireLock (lockPitchEg, "Lock pitch EG to the lock-reference point");
    wireLock (lockLfo, "Lock DX7 LFO block to the lock-reference point");
    wireLock (lockScaling, "Lock keyboard level scale + rate scaling to the lock-reference point");
    wireLock (lockSens, "Lock AMS and key-velocity sensitivity to the lock-reference point");
    wireLock (lockTranspose, "Lock transpose to the lock-reference point");

    resetLocksBtn.setTooltip ("Restore lock groups and lock reference to the saved defaults (Set default)");
    resetLocksBtn.onClick = [this] { resetLocksToDefaults(); };
    addAndMakeVisible (resetLocksBtn);
    setDefaultLocksBtn.setTooltip ("Save current locks and lock-reference as defaults (used by Reset)");
    setDefaultLocksBtn.onClick = [this] { saveLocksAsDefaults(); };
    addAndMakeVisible (setDefaultLocksBtn);

    lfoToggle.setClickingTogglesState (true);
    lfoToggle.setTooltip ("Walk the pad edges (A-B-D-C) continuously. Turns off Note morph when enabled. While on, left-click does not move the marker; right-drag still sets lock ref.");
    lfoToggle.onClick = [this]
    {
        if (hostParamSync)
            return;
        const bool on = lfoToggle.getToggleState();
        const int choice = on
                               ? static_cast<int> (fmlib::MorphMotionMode::edgeLfo)
                               : static_cast<int> (fmlib::MorphMotionMode::off);
        lfoEnabled = on;
        if (on && noteJumpMode != NoteJumpMode::off)
        {
            noteJumpMode = NoteJumpMode::off;
            syncNoteJumpUi();
        }
        if (onMorphMotionHostWrite)
            onMorphMotionHostWrite (choice);
        else
            setLfoEnabled (on);
        if (on)
            resumeEgressIfPaused();
        if (onMorphUiPrefsChanged)
            onMorphUiPrefsChanged();
    };
    addAndMakeVisible (lfoToggle);

    lfoRate.setRange (-1.5, 1.5, 0.01);
    lfoRate.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoRate.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    lfoRate.setTextValueSuffix (" Hz");
    lfoRate.setValue (0.25, juce::dontSendNotification);
    lfoRate.setTooltip ("Edge LFO rate in Hz: positive = clockwise, negative = counter-clockwise. Near 0 = paused.");
    lfoRate.onValueChange = [this]
    {
        if (hostParamSync)
            return;
        lfoRateHz = (float) lfoRate.getValue();
        if (onMorphLfoRateHostWrite)
            onMorphLfoRateHostWrite (lfoRateHz);
        if (onMorphUiPrefsChanged)
            onMorphUiPrefsChanged();
    };
    addAndMakeVisible (lfoRate);
    addAndMakeVisible (lfoRateLabel);

    lfoSync.setClickingTogglesState (true);
    lfoSync.setTooltip ("Lock Edge LFO speed to the DAW tempo. Use CCW for counter-clockwise orbits.");
    lfoSync.onClick = [this]
    {
        if (hostParamSync)
            return;
        lfoTempoSync = lfoSync.getToggleState();
        if (onMorphLfoSyncHostWrite)
            onMorphLfoSyncHostWrite (lfoTempoSync);
        syncLfoUi();
        if (onMorphUiPrefsChanged)
            onMorphUiPrefsChanged();
    };
    addAndMakeVisible (lfoSync);

    lfoCcw.setClickingTogglesState (true);
    lfoCcw.setTooltip ("Counter-clockwise pad orbit while Sync is on. Off = clockwise (A-B-D-C). Stored as the sign of Edge LFO Rate.");
    lfoCcw.onClick = [this]
    {
        if (hostParamSync)
            return;
        lfoRateHz = rateWithClockwise (lfoRateHz, ! lfoCcw.getToggleState());
        lfoRate.setValue (lfoRateHz, juce::dontSendNotification);
        if (onMorphLfoRateHostWrite)
            onMorphLfoRateHostWrite (lfoRateHz);
        if (onMorphUiPrefsChanged)
            onMorphUiPrefsChanged();
    };
    addChildComponent (lfoCcw);

    lfoDivision.addItemList (morphLfoDivisionLabels(), 1);
    lfoDivision.setSelectedItemIndex (kMorphLfoDivisionDefault, juce::dontSendNotification);
    lfoDivision.setTooltip ("Note length of one pad edge (A-B-D-C has four edges). T = triplet, D = dotted. At 1/4, one full orbit takes a bar of 4/4. CCW reverses the orbit. Follows host BPM; phase locks to the playhead while the transport runs.");
    lfoDivision.onChange = [this]
    {
        if (hostParamSync)
            return;
        lfoDivisionChoice = juce::jmax (0, lfoDivision.getSelectedItemIndex());
        if (onMorphLfoDivisionHostWrite)
            onMorphLfoDivisionHostWrite (lfoDivisionChoice);
        if (onMorphUiPrefsChanged)
            onMorphUiPrefsChanged();
    };
    addChildComponent (lfoDivision);

    noteJumpLabel.setJustificationType (juce::Justification::centredLeft);
    noteJumpLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (noteJumpLabel);

    auto wireNoteJump = [this] (juce::ToggleButton& b, NoteJumpMode m, const juce::String& tip)
    {
        b.setClickingTogglesState (true);
        b.setRadioGroupId (7701);
        b.setTooltip (tip);
        b.onClick = [this, m]
        {
            if (noteJumpMode == m)
                return;
            const auto prev = noteJumpMode;
            noteJumpMode = m;
            if (m != NoteJumpMode::off)
                lfoEnabled = false;
            const int choice = static_cast<int> (m);
            if (onMorphMotionHostWrite)
                onMorphMotionHostWrite (choice);
            else
                setNoteJumpMode (m);
            if (m != NoteJumpMode::off)
                resumeEgressIfPaused();
            if (onMorphUiPrefsChanged)
                onMorphUiPrefsChanged();
            if (prev == NoteJumpMode::off && m != NoteJumpMode::off && onNoteJumpArmed)
                onNoteJumpArmed();
        };
        addAndMakeVisible (b);
    };
    wireNoteJump (noteJumpOff, NoteJumpMode::off, "No pad jump from Audition / controller notes");
    wireNoteJump (noteJumpRandom, NoteJumpMode::random,
                  "On each new phrase (first key), jump to a random pad point. Turns off Edge LFO.");
    wireNoteJump (noteJumpEdges, NoteJumpMode::edges,
                  "On each new phrase (first key), cycle the corners A, B, D, C. Turns off Edge LFO.");
    syncNoteJumpUi();

    updateHint();
    updateMorphControlsEnabled();

    presetName.setTextToShowWhenEmpty ("Preset name", juce::Colours::grey);
    presetName.setTooltip ("Name for the next saved preset. Left empty, the generated morph name is used.");
    savePreset.setTooltip ("Save the current corners, pad position and locks as a morph preset.");
    deletePreset.setTooltip ("Delete the selected morph preset.");
    deviceSlotLabel.setTooltip ("Target slot (1-32) in the plugin Device buffer.");
    addAndMakeVisible (presetName);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (deletePreset);
    addAndMakeVisible (deviceSlotLabel);
    addAndMakeVisible (deviceSlot);
    addAndMakeVisible (saveToDevice);
    addAndMakeVisible (presetList);
    for (auto* c : std::initializer_list<juce::Component*> { &presetList, &presetName, &savePreset, &deletePreset,
                                                             &deviceSlotLabel, &deviceSlot, &saveToDevice })
        c->setVisible (false);

    deviceSlotLabel.setJustificationType (juce::Justification::centredLeft);
    for (int i = 1; i <= 32; ++i)
        deviceSlot.addItem (juce::String (i), i);
    deviceSlot.setSelectedId (1, juce::dontSendNotification);
    saveToDevice.setTooltip ("Copy the selected morph preset voice into the plugin Device buffer at the chosen slot (1-32).");
    presetList.setTooltip ("Click or arrow keys: load preset. Double-click: Morph pad. Right-click: load and Open in folder.");
    savePreset.onClick = [this] { saveCurrentPreset(); };
    deletePreset.onClick = [this] { deleteSelectedPreset(); };
    saveToDevice.onClick = [this] { saveSelectedPresetToDeviceSlot(); };
}

MorpherPanel::~MorpherPanel()
{
    stopTimer();
}

void MorpherPanel::setEmitIntervalMs (int ms)
{
    emitIntervalMs = juce::jlimit (20, 250, ms);
}

int MorpherPanel::effectiveEmitIntervalMs() const
{
    // Transport owns wire budgeting; this only coalesces UI-side emitMorph calls.
    return emitIntervalMs;
}

void MorpherPanel::setLockGroups (uint32_t groups)
{
    lockGroups = groups & morphLockAllGroups;
    syncLockChipsFromFlags();
    lastSentVoice.reset();
    emitMorph (false);
    repaint();
}

void MorpherPanel::setDefaultLockGroups (uint32_t groups)
{
    defaultLockGroups = groups & morphLockAllGroups;
}

void MorpherPanel::setLockRefPosition (float x, float y)
{
    lockRefX = juce::jlimit (0.0f, 1.0f, x);
    lockRefY = juce::jlimit (0.0f, 1.0f, y);
    lastSentVoice.reset();
    emitMorph (false, true);
    repaint();
}

void MorpherPanel::setEgressPaused (bool paused)
{
    egressPaused = paused;
    if (! egressPaused)
        ensureTimerRunning();
}

void MorpherPanel::resumeEgressIfPaused()
{
    if (! egressPaused)
        return;
    if (onPadGestureStarted)
        onPadGestureStarted();
    else
        setEgressPaused (false);
}

void MorpherPanel::setDefaultLockRefPosition (float x, float y)
{
    defaultLockRefX = juce::jlimit (0.0f, 1.0f, x);
    defaultLockRefY = juce::jlimit (0.0f, 1.0f, y);
}

void MorpherPanel::setLfoEnabled (bool on)
{
    lfoEnabled = on;
    lastLfoStep = -1;
    if (lfoEnabled)
    {
        lastTimerMs = juce::Time::getMillisecondCounter();
        if (noteJumpMode != NoteJumpMode::off)
        {
            noteJumpMode = NoteJumpMode::off;
            syncNoteJumpUi();
        }
        resumeEgressIfPaused();
    }
    syncLfoUi();
    updateHint();
    ensureTimerRunning();
    repaint();
}

void MorpherPanel::setLfoRateHz (float hz)
{
    lfoRateHz = juce::jlimit (-1.5f, 1.5f, hz);
    syncLfoUi();
}

void MorpherPanel::setNoteJumpMode (NoteJumpMode m)
{
    noteJumpMode = m;
    if (noteJumpMode != NoteJumpMode::off && lfoEnabled)
    {
        lfoEnabled = false;
        lastLfoStep = -1;
        syncLfoUi();
        ensureTimerRunning();
    }
    if (noteJumpMode != NoteJumpMode::off)
        resumeEgressIfPaused();
    syncNoteJumpUi();
    updateHint();
}

void MorpherPanel::syncNoteJumpUi()
{
    noteJumpOff.setToggleState (noteJumpMode == NoteJumpMode::off, juce::dontSendNotification);
    noteJumpRandom.setToggleState (noteJumpMode == NoteJumpMode::random, juce::dontSendNotification);
    noteJumpEdges.setToggleState (noteJumpMode == NoteJumpMode::edges, juce::dontSendNotification);
}

bool MorpherPanel::applyNoteJump()
{
    switch (noteJumpMode)
    {
        case NoteJumpMode::off:
            return false;
        case NoteJumpMode::random:
            jumpToRandomPosition();
            return true;
        case NoteJumpMode::edges:
            jumpToNextEdge();
            return true;
    }
    return false;
}

void MorpherPanel::syncLfoUi (bool relayout)
{
    lfoToggle.setToggleState (lfoEnabled, juce::dontSendNotification);
    lfoSync.setToggleState (lfoTempoSync, juce::dontSendNotification);
    lfoRate.setValue (lfoRateHz, juce::dontSendNotification);
    lfoCcw.setToggleState (lfoRateHz < 0.0f, juce::dontSendNotification);
    lfoDivision.setSelectedItemIndex (clampMorphLfoDivision (lfoDivisionChoice), juce::dontSendNotification);
    lfoRate.setVisible (mode == Mode::morph && ! lfoTempoSync);
    lfoRateLabel.setVisible (mode == Mode::morph && ! lfoTempoSync);
    lfoCcw.setVisible (mode == Mode::morph && lfoTempoSync);
    lfoDivision.setVisible (mode == Mode::morph && lfoTempoSync);
    if (relayout)
        resized();
}

void MorpherPanel::syncLockChipsFromFlags()
{
    lockEg.setToggleState ((lockGroups & morphLockEg) != 0, juce::dontSendNotification);
    lockLevels.setToggleState ((lockGroups & morphLockLevels) != 0, juce::dontSendNotification);
    lockFreqCoarse.setToggleState ((lockGroups & morphLockFreqCoarse) != 0, juce::dontSendNotification);
    lockFreqFine.setToggleState ((lockGroups & morphLockFreqFine) != 0, juce::dontSendNotification);
    lockAlgo.setToggleState ((lockGroups & morphLockAlgo) != 0, juce::dontSendNotification);
    lockFeedback.setToggleState ((lockGroups & morphLockFeedback) != 0, juce::dontSendNotification);
    lockSync.setToggleState ((lockGroups & morphLockOscSync) != 0, juce::dontSendNotification);
    lockPitchEg.setToggleState ((lockGroups & morphLockPitchEg) != 0, juce::dontSendNotification);
    lockLfo.setToggleState ((lockGroups & morphLockLfo) != 0, juce::dontSendNotification);
    lockScaling.setToggleState ((lockGroups & morphLockScaling) != 0, juce::dontSendNotification);
    lockSens.setToggleState ((lockGroups & morphLockSens) != 0, juce::dontSendNotification);
    lockTranspose.setToggleState ((lockGroups & morphLockTranspose) != 0, juce::dontSendNotification);
}

void MorpherPanel::applyLockChipToggles()
{
    uint32_t g = morphLockNone;
    if (lockEg.getToggleState()) g |= morphLockEg;
    if (lockLevels.getToggleState()) g |= morphLockLevels;
    if (lockFreqCoarse.getToggleState()) g |= morphLockFreqCoarse;
    if (lockFreqFine.getToggleState()) g |= morphLockFreqFine;
    if (lockAlgo.getToggleState()) g |= morphLockAlgo;
    if (lockFeedback.getToggleState()) g |= morphLockFeedback;
    if (lockSync.getToggleState()) g |= morphLockOscSync;
    if (lockPitchEg.getToggleState()) g |= morphLockPitchEg;
    if (lockLfo.getToggleState()) g |= morphLockLfo;
    if (lockScaling.getToggleState()) g |= morphLockScaling;
    if (lockSens.getToggleState()) g |= morphLockSens;
    if (lockTranspose.getToggleState()) g |= morphLockTranspose;
    lockGroups = g;
    lastSentVoice.reset();
    emitMorph (false, true);
    if (onMorphUiPrefsChanged)
        onMorphUiPrefsChanged();
}

void MorpherPanel::saveLocksAsDefaults()
{
    defaultLockGroups = lockGroups;
    defaultLockRefX = lockRefX;
    defaultLockRefY = lockRefY;
    if (onMorphUiPrefsChanged)
        onMorphUiPrefsChanged();
    if (onStatus)
        onStatus ("Lock defaults saved");
}

void MorpherPanel::resetLocksToDefaults()
{
    lockGroups = defaultLockGroups;
    lockRefX = defaultLockRefX;
    lockRefY = defaultLockRefY;
    syncLockChipsFromFlags();
    lastSentVoice.reset();
    emitMorph (false, true);
    if (onMorphUiPrefsChanged)
        onMorphUiPrefsChanged();
    if (onStatus)
        onStatus ("Locks restored to defaults");
    repaint();
}

void MorpherPanel::setMode (Mode m)
{
    mode = m;
    const bool morph = mode == Mode::morph;
    modeMorph.setToggleState (morph, juce::dontSendNotification);
    modePresets.setToggleState (! morph, juce::dontSendNotification);

    for (auto* c : { &assignA, &assignB, &assignC, &assignD, &clearBtn })
        c->setVisible (morph);
    hint.setVisible (morph);
    locksLabel.setVisible (morph);
    for (auto* c : { &lockEg, &lockLevels, &lockFreqCoarse, &lockFreqFine, &lockAlgo, &lockFeedback, &lockSync,
                     &lockPitchEg, &lockLfo, &lockScaling, &lockSens, &lockTranspose })
        c->setVisible (morph);
    resetLocksBtn.setVisible (morph);
    setDefaultLocksBtn.setVisible (morph);
    lfoToggle.setVisible (morph);
    lfoSync.setVisible (morph);
    lfoRate.setVisible (morph && ! lfoTempoSync);
    lfoRateLabel.setVisible (morph && ! lfoTempoSync);
    lfoCcw.setVisible (morph && lfoTempoSync);
    lfoDivision.setVisible (morph && lfoTempoSync);
    noteJumpLabel.setVisible (morph);
    noteJumpOff.setVisible (morph);
    noteJumpRandom.setVisible (morph);
    noteJumpEdges.setVisible (morph);

    presetList.setVisible (! morph);
    presetName.setVisible (! morph);
    savePreset.setVisible (! morph);
    deletePreset.setVisible (! morph);
    deviceSlotLabel.setVisible (! morph);
    deviceSlot.setVisible (! morph);
    saveToDevice.setVisible (! morph);
    if (! morph)
        refreshPresetList();
    ensureTimerRunning();
    updateMorphControlsEnabled();
    resized();
    if (! morph)
        refreshPresetList();
    repaint();
}

bool MorpherPanel::allCornersReady() const
{
    return cornerSet[0] && cornerSet[1] && cornerSet[2] && cornerSet[3];
}

VoiceData MorpherPanel::morphAt (float x, float y) const
{
    return VoiceMorpher::morph4 (corners[0], corners[1], corners[2], corners[3], x, y,
                                 lockGroups, lockRefX, lockRefY);
}

VoiceData MorpherPanel::morphAtCurrent() const
{
    return morphAt (posX, posY);
}

void MorpherPanel::updateHint()
{
    if (allCornersReady())
    {
        if (lfoEnabled)
            hint.setText ("Edge LFO drives the marker. Right-drag = lock ref (updates the voice when you release, including held notes).",
                          juce::dontSendNotification);
        else
            hint.setText ("Drag to morph. Right-drag = lock ref (updates the voice when you release, including held notes).",
                          juce::dontSendNotification);
    }
    else
        hint.setText ("Select a library voice, Set all four corners (A-D). Pad, LFO and Note morph stay off until then.",
                      juce::dontSendNotification);
}

void MorpherPanel::updateMorphControlsEnabled()
{
    const bool ready = allCornersReady();
    const bool morph = mode == Mode::morph;
    const bool on = morph && ready;

    lfoToggle.setEnabled (on);
    lfoSync.setEnabled (on);
    lfoCcw.setEnabled (on);
    lfoRate.setEnabled (on);
    lfoRateLabel.setEnabled (on);
    lfoDivision.setEnabled (on);
    for (auto* c : { &noteJumpOff, &noteJumpRandom, &noteJumpEdges })
        c->setEnabled (on);
    noteJumpLabel.setEnabled (on);
    // Pad input is gated in mouse handlers; locks/assign stay usable while building corners.
}

juce::String MorpherPanel::cornerDisplayName (int corner) const
{
    if (corner < 0 || corner > 3)
        return {};
    const auto letter = juce::String::charToString (static_cast<juce::juce_wchar> ('A' + corner));
    if (! cornerSet[corner] || cornerNames[corner].isEmpty())
        return letter + ": (unset)";
    return letter + ": " + cornerNames[corner];
}

int MorpherPanel::percent99 (float t)
{
    return juce::jlimit (0, 99, (int) std::lround ((double) juce::jlimit (0.0f, 1.0f, t) * 99.0));
}

void MorpherPanel::setCorner (int corner0to3, const VoiceData& v, const juce::String& name)
{
    if (corner0to3 < 0 || corner0to3 > 3)
        return;
    corners[corner0to3] = v;
    cornerSet[corner0to3] = true;
    cornerNames[corner0to3] = name.isNotEmpty() ? name : "Voice";
    lastSentVoice.reset();
    if (onMorphInvalidate)
        onMorphInvalidate();
    updateHint();
    updateMorphControlsEnabled();
    emitMorph (false);
    repaint();
}

void MorpherPanel::clearCorners()
{
    for (int i = 0; i < 4; ++i)
    {
        corners[i] = {};
        cornerSet[i] = false;
        cornerNames[i].clear();
    }
    posX = 0.0f;
    posY = 0.0f;
    lfoPhase = 0.0f;
    lastLfoStep = -1;
    lastSentVoice.reset();
    if (onMorphInvalidate)
        onMorphInvalidate();
    if (onCornersCleared)
        onCornersCleared();
    updateHint();
    updateMorphControlsEnabled();
    repaint();
}

void MorpherPanel::setMarkerPosition (float x, float y)
{
    posX = juce::jlimit (0.0f, 1.0f, x);
    posY = juce::jlimit (0.0f, 1.0f, y);
    if (processorOwnsMotion && onMorphPositionHostWrite)
        onMorphPositionHostWrite (posX, posY, false, false);
    else
        emitMorph (false);
    repaint();
}

void MorpherPanel::setMarkerFromHost (float x, float y)
{
    hostParamSync = true;
    posX = juce::jlimit (0.0f, 1.0f, x);
    posY = juce::jlimit (0.0f, 1.0f, y);
    lastSentVoice.reset();
    hostParamSync = false;
    repaint();
}

void MorpherPanel::setLockRefFromHost (float x, float y)
{
    hostParamSync = true;
    lockRefX = juce::jlimit (0.0f, 1.0f, x);
    lockRefY = juce::jlimit (0.0f, 1.0f, y);
    hostParamSync = false;
    repaint();
}

void MorpherPanel::syncMotionFromHost (int motionChoice, float rateHz, bool tempoSync, int division)
{
    hostParamSync = true;
    const auto mapping = morphMotionFromChoice (motionChoice);
    lfoEnabled = mapping.lfoEnabled;
    noteJumpMode = static_cast<NoteJumpMode> (juce::jlimit (0, 2, mapping.noteJumpMode));
    lfoRateHz = juce::jlimit (-1.5f, 1.5f, rateHz);
    lfoTempoSync = tempoSync;
    lfoDivisionChoice = clampMorphLfoDivision (division);
    syncLfoUi (false);
    syncNoteJumpUi();
    updateHint();
    updateMorphControlsEnabled();
    if (! processorOwnsMotion)
        ensureTimerRunning();
    hostParamSync = false;
    repaint();
}

void MorpherPanel::applyPresetSnapshot (const MorphPreset& p)
{
    corners[0] = p.a;
    corners[1] = p.b;
    corners[2] = p.c;
    corners[3] = p.d;
    cornerSet[0] = cornerSet[1] = cornerSet[2] = cornerSet[3] = true;
    cornerNames[0] = juce::String::fromUTF8 (p.nameA.c_str());
    cornerNames[1] = juce::String::fromUTF8 (p.nameB.c_str());
    cornerNames[2] = juce::String::fromUTF8 (p.nameC.c_str());
    cornerNames[3] = juce::String::fromUTF8 (p.nameD.c_str());
    setMarkerFromHost (p.posX, p.posY);
    lockGroups = p.lockGroups & morphLockAllGroups;
    setLockRefFromHost (p.lockRefX, p.lockRefY);
    syncLockChipsFromFlags();
    lastSentVoice.reset();
    lastLfoStep = -1;
    lastTimerMs = juce::Time::getMillisecondCounter();
    if (onMorphInvalidate)
        onMorphInvalidate();
    egressPaused = false;
    updateHint();
    updateMorphControlsEnabled();
    ensureTimerRunning();
    repaint();
}

void MorpherPanel::jumpToRandomPosition()
{
    if (mode != Mode::morph)
        setMode (Mode::morph);
    setMarkerPosition (juce::Random::getSystemRandom().nextFloat(),
                       juce::Random::getSystemRandom().nextFloat());
}

void MorpherPanel::jumpToNextEdge()
{
    if (mode != Mode::morph)
        setMode (Mode::morph);
    // A(0,0) -> B(1,0) -> D(1,1) -> C(0,1)
    static constexpr float kEdges[4][2] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };
    const auto& e = kEdges[static_cast<size_t> (edgeJumpIndex & 3)];
    edgeJumpIndex = (edgeJumpIndex + 1) & 3;
    setMarkerPosition (e[0], e[1]);
}

void MorpherPanel::setPresetStore (MorphPresetStore* s)
{
    store = s;
    refreshPresetList();
}

MorphPreset MorpherPanel::currentAsPreset (const juce::String& name) const
{
    MorphPreset p;
    p.name = name.toStdString();
    p.a = corners[0];
    p.b = corners[1];
    p.c = corners[2];
    p.d = corners[3];
    p.nameA = cornerNames[0].toStdString();
    p.nameB = cornerNames[1].toStdString();
    p.nameC = cornerNames[2].toStdString();
    p.nameD = cornerNames[3].toStdString();
    p.posX = posX;
    p.posY = posY;
    p.lockGroups = lockGroups;
    p.lockRefX = lockRefX;
    p.lockRefY = lockRefY;
    return p;
}

void MorpherPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto modeRow = r.removeFromTop (28);
    modeMorph.setBounds (modeRow.removeFromLeft (90).reduced (2));
    modePresets.setBounds (modeRow.removeFromLeft (90).reduced (2));
    r.removeFromTop (4);

    if (mode == Mode::morph)
    {
        auto assigns = r.removeFromTop (26);
        assignA.setBounds (assigns.removeFromLeft (58).reduced (1));
        assignB.setBounds (assigns.removeFromLeft (58).reduced (1));
        assignC.setBounds (assigns.removeFromLeft (58).reduced (1));
        assignD.setBounds (assigns.removeFromLeft (58).reduced (1));
        clearBtn.setBounds (assigns.removeFromLeft (64).reduced (1));

        // Leave room for LFO + Note morph + hint + a usable pad.
        constexpr int kMorphChromeBelowLocks = 26 + 26 + 20 + 120;
        layoutLockChips (r, kMorphChromeBelowLocks);

        constexpr int toggleRowH = 26;
        auto lfoRow = r.removeFromTop (toggleRowH);
        lfoToggle.setBounds (lfoRow.removeFromLeft (toggleWidthFor (lfoToggle, toggleRowH)).reduced (1));
        lfoSync.setBounds (lfoRow.removeFromLeft (toggleWidthFor (lfoSync, toggleRowH)).reduced (1));
        if (lfoTempoSync)
        {
            lfoRateLabel.setBounds ({});
            lfoRate.setBounds ({});
            lfoCcw.setBounds (lfoRow.removeFromLeft (toggleWidthFor (lfoCcw, toggleRowH)).reduced (1));
            lfoDivision.setBounds (lfoRow.reduced (1));
        }
        else
        {
            lfoCcw.setBounds ({});
            lfoDivision.setBounds ({});
            lfoRateLabel.setBounds (lfoRow.removeFromLeft (36));
            lfoRate.setBounds (lfoRow.reduced (1));
        }

        auto jumpRow = r.removeFromTop (toggleRowH);
        noteJumpLabel.setBounds (jumpRow.removeFromLeft (78));
        for (auto* b : { &noteJumpOff, &noteJumpRandom, &noteJumpEdges })
            b->setBounds (jumpRow.removeFromLeft (toggleWidthFor (*b, toggleRowH)).reduced (1));

        hint.setBounds (r.removeFromTop (20));
        auto area = r.reduced (6);
        const int side = juce::jmin (area.getWidth(), area.getHeight());
        padBounds = { area.getCentreX() - side / 2, area.getCentreY() - side / 2, side, side };
    }
    else
    {
        constexpr int rowH = 30;
        auto nameRow = r.removeFromTop (rowH);
        savePreset.setBounds (nameRow.removeFromRight (90).reduced (2));
        deletePreset.setBounds (nameRow.removeFromRight (80).reduced (2));
        nameRow.removeFromRight (8);
        presetName.setBounds (nameRow.reduced (2));

        r.removeFromTop (2);
        auto slotRow = r.removeFromTop (rowH);
        saveToDevice.setBounds (slotRow.removeFromRight (170).reduced (2));
        slotRow.removeFromRight (8);
        deviceSlot.setBounds (slotRow.removeFromRight (72).reduced (2));
        deviceSlotLabel.setBounds (slotRow.reduced (2));

        r.removeFromTop (6);
        presetList.setBounds (r);
        padBounds = {};
        refreshPresetList();
    }
}

void MorpherPanel::layoutLockChips (juce::Rectangle<int>& area, int minLeaveBelow)
{
    constexpr int rowH = 24;
    constexpr int labelW = 44;

    auto takeChipRow = [&]() -> juce::Rectangle<int>
    {
        if (area.getHeight() < rowH + juce::jmax (0, minLeaveBelow))
            return {};
        return area.removeFromTop (rowH);
    };

    auto row = takeChipRow();
    if (row.isEmpty())
    {
        locksLabel.setBounds ({});
        resetLocksBtn.setBounds ({});
        setDefaultLocksBtn.setBounds ({});
        for (auto* chip : { &lockEg, &lockLevels, &lockFreqCoarse, &lockFreqFine, &lockAlgo, &lockFeedback,
                            &lockPitchEg, &lockLfo, &lockScaling, &lockSens, &lockTranspose, &lockSync })
            chip->setBounds ({});
        return;
    }

    locksLabel.setBounds (row.removeFromLeft (labelW));
    setDefaultLocksBtn.setBounds (row.removeFromRight (92).reduced (1));
    resetLocksBtn.setBounds (row.removeFromRight (60).reduced (1));

    bool outOfRoom = false;
    for (auto* chip : { &lockEg, &lockLevels, &lockFreqCoarse, &lockFreqFine, &lockAlgo, &lockFeedback,
                        &lockPitchEg, &lockLfo, &lockScaling, &lockSens, &lockTranspose, &lockSync })
    {
        if (outOfRoom)
        {
            chip->setBounds ({});
            continue;
        }
        const int w = toggleWidthFor (*chip, rowH);
        if (w > row.getWidth() || row.getWidth() <= 0)
        {
            row = takeChipRow();
            if (row.isEmpty())
            {
                chip->setBounds ({});
                outOfRoom = true;
                continue;
            }
            row.removeFromLeft (labelW);
        }
        chip->setBounds (row.removeFromLeft (juce::jmin (w, juce::jmax (1, row.getWidth()))).reduced (1));
    }
}

void MorpherPanel::lookAndFeelChanged()
{
    const auto bg = findColour (juce::ResizableWindow::backgroundColourId);
    const bool darkTheme = bg.getBrightness() <= 0.5f;
    const auto p = ThemePalette::forTheme (darkTheme);
    lfoRate.setColour (juce::Slider::backgroundColourId, p.raised);
    lfoRate.setColour (juce::Slider::thumbColourId, p.accent);
    lfoRate.setColour (juce::Slider::trackColourId, p.accent.withMultipliedAlpha (0.65f));
    lfoRate.setColour (juce::Slider::textBoxTextColourId, p.text);
    lfoRate.setColour (juce::Slider::textBoxBackgroundColourId, p.widget);
    lfoRate.setColour (juce::Slider::textBoxOutlineColourId, p.outline);
}

void MorpherPanel::paint (juce::Graphics& g)
{
    if (mode != Mode::morph || padBounds.isEmpty())
        return;

    g.setColour (findColour (juce::TextEditor::backgroundColourId));
    g.fillRect (padBounds);

    const auto textColour = findColour (juce::Label::textColourId);
    const float midX = (float) padBounds.getCentreX();
    const float midY = (float) padBounds.getCentreY();
    g.setColour (textColour.withMultipliedAlpha (0.15f));
    g.drawLine ((float) padBounds.getX(), midY, (float) padBounds.getRight(), midY, 1.0f);
    g.drawLine (midX, (float) padBounds.getY(), midX, (float) padBounds.getBottom(), 1.0f);

    if (lfoEnabled)
    {
        g.setColour (findColour (juce::TextButton::buttonOnColourId).withMultipliedAlpha (0.55f));
        const float ax = (float) padBounds.getX() + 4.0f;
        const float ay = (float) padBounds.getY() + 4.0f;
        const float bx = (float) padBounds.getRight() - 4.0f;
        const float by = (float) padBounds.getBottom() - 4.0f;
        juce::Path path;
        path.startNewSubPath (ax, ay);
        path.lineTo (bx, ay);
        path.lineTo (bx, by);
        path.lineTo (ax, by);
        path.closeSubPath();
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    g.setColour (textColour);
    g.drawRect (padBounds, 1);

    const float rx = (float) padBounds.getX() + lockRefX * (float) padBounds.getWidth();
    const float ry = (float) padBounds.getY() + lockRefY * (float) padBounds.getHeight();

    if (lockRefDragging)
    {
        drawPadCrosshair (g, rx, ry, textColour.withMultipliedAlpha (0.4f));
        drawPadCoordLabel (g, rx, ry,
                           "Ref (" + juce::String (percent99 (lockRefX)) + "%, "
                               + juce::String (percent99 (lockRefY)) + "%)",
                           textColour.withMultipliedAlpha (0.9f));
    }

    g.setColour (textColour.withMultipliedAlpha (lockRefDragging ? 0.95f : 0.75f));
    g.drawEllipse (rx - 8.0f, ry - 8.0f, 16.0f, 16.0f, 1.5f);
    g.drawLine (rx - 5.0f, ry, rx + 5.0f, ry, 1.0f);
    g.drawLine (rx, ry - 5.0f, rx, ry + 5.0f, 1.0f);

    const float x = (float) padBounds.getX() + posX * (float) padBounds.getWidth();
    const float y = (float) padBounds.getY() + posY * (float) padBounds.getHeight();

    if (padDragging)
    {
        drawPadCrosshair (g, x, y, textColour.withMultipliedAlpha (0.35f));
        drawPadCoordLabel (g, x, y,
                           "(" + juce::String (percent99 (posX)) + "%, "
                               + juce::String (percent99 (posY)) + "%)",
                           textColour.withMultipliedAlpha (0.85f));
    }

    g.setColour (findColour (juce::TextButton::buttonOnColourId));
    g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);

    const auto dimColour = textColour.withMultipliedAlpha (0.45f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const int labelW = juce::jmax (80, padBounds.getWidth() / 2 - 12);
    const int labelH = 36;

    auto drawCorner = [&] (int corner, int x0, int y0, juce::Justification j)
    {
        g.setColour (cornerSet[static_cast<size_t> (corner)] ? textColour : dimColour);
        g.drawText (cornerDisplayName (corner), x0, y0, labelW, labelH, j, true);
    };
    drawCorner (0, padBounds.getX() + 6, padBounds.getY() + 6, juce::Justification::topLeft);
    drawCorner (1, padBounds.getRight() - labelW - 6, padBounds.getY() + 6, juce::Justification::topRight);
    drawCorner (2, padBounds.getX() + 6, padBounds.getBottom() - labelH - 6, juce::Justification::bottomLeft);
    drawCorner (3, padBounds.getRight() - labelW - 6, padBounds.getBottom() - labelH - 6, juce::Justification::bottomRight);
}

void MorpherPanel::drawPadCrosshair (juce::Graphics& g, float x, float y, juce::Colour colour) const
{
    g.setColour (colour);
    g.drawLine ((float) padBounds.getX(), y, (float) padBounds.getRight(), y, 1.0f);
    g.drawLine (x, (float) padBounds.getY(), x, (float) padBounds.getBottom(), 1.0f);
}

void MorpherPanel::drawPadCoordLabel (juce::Graphics& g, float x, float y, const juce::String& text,
                                      juce::Colour colour) const
{
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colour);
    const int tw = juce::jmax (72, (int) std::ceil (juce::GlyphArrangement::getStringWidth (
                                                        juce::Font (juce::FontOptions (11.0f)), text))
                                    + 4);
    const int th = 16;
    int tx = (int) x + 10;
    int ty = (int) y - 18;
    if (tx + tw > padBounds.getRight() - 4)
        tx = (int) x - tw - 10;
    if (ty < padBounds.getY() + 4)
        ty = (int) y + 10;
    if (ty + th > padBounds.getBottom() - 4)
        ty = padBounds.getBottom() - th - 4;
    g.drawText (text, tx, ty, tw, th, juce::Justification::centredLeft, false);
}

void MorpherPanel::updateLockRefFromPoint (juce::Point<float> p)
{
    lockRefX = juce::jlimit (0.0f, 1.0f, (p.x - (float) padBounds.getX()) / (float) padBounds.getWidth());
    lockRefY = juce::jlimit (0.0f, 1.0f, (p.y - (float) padBounds.getY()) / (float) padBounds.getHeight());
    // Next morph emit should recompute against the new reference (no MIDI while placing).
    lastSentVoice.reset();
    repaint();
}

void MorpherPanel::commitLockRef()
{
    if (onMorphUiPrefsChanged)
        onMorphUiPrefsChanged();
    lastSentVoice.reset();
    if (processorOwnsMotion && onLockRefHostWrite)
    {
        onLockRefHostWrite (lockRefX, lockRefY, false, true);
        if (onStatus)
            onStatus ("Lock reference @ " + juce::String (percent99 (lockRefX)) + "%, "
                      + juce::String (percent99 (lockRefY)) + "%");
        return;
    }
    if (egressPaused)
    {
        if (onStatus)
            onStatus ("Lock reference @ " + juce::String (percent99 (lockRefX)) + "%, "
                      + juce::String (percent99 (lockRefY))
                      + "% (morph paused — applies on resume)");
        return;
    }
    emitMorph (false, true);
    if (onStatus)
        onStatus ("Lock reference @ " + juce::String (percent99 (lockRefX)) + "%, "
                  + juce::String (percent99 (lockRefY))
                  + "% (locked groups follow Ref; voice updated)");
}

bool MorpherPanel::padContains (juce::Point<float> p) const
{
    return padBounds.toFloat().contains (p);
}

void MorpherPanel::cancelPadGesture()
{
    const bool wasLive = padGesture || lockRefDragging || padDragging;
    padGesture = false;
    padDragging = false;
    lockRefDragging = false;
    pendingMorph = false;
    if (wasLive)
        repaint();
}

void MorpherPanel::mouseDown (const juce::MouseEvent& e)
{
    if (mode != Mode::morph || padBounds.isEmpty())
        return;

    if (! padContains (e.position))
    {
        cancelPadGesture();
        return;
    }

    if (! allCornersReady())
    {
        cancelPadGesture();
        return;
    }

    if (e.mods.isPopupMenu())
    {
        padGesture = true;
        lockRefDragging = true;
        padDragging = false;
        updateLockRefFromPoint (e.position);
        return;
    }

    if (lfoEnabled)
    {
        cancelPadGesture();
        return;
    }

    padGesture = true;
    lockRefDragging = false;
    padDragging = false;
    pendingMorph = false;
    if (! lfoEnabled)
        stopTimer();
    if (egressPaused)
        setEgressPaused (false);
    if (onPadGestureStarted)
        onPadGestureStarted();
    padDownPos = e.position;
    updatePositionFromPoint (e.position);
    if (processorOwnsMotion && onMorphPositionHostWrite)
        onMorphPositionHostWrite (posX, posY, true, false);
    repaint();
}

void MorpherPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! padGesture || mode != Mode::morph || padBounds.isEmpty())
        return;

    if (lockRefDragging)
    {
        updateLockRefFromPoint (e.position);
        return;
    }

    if (e.mods.isPopupMenu())
        return;

    if (! padDragging && e.position.getDistanceFrom (padDownPos) >= kPadDragThresholdPx)
        padDragging = true;

    updatePositionFromPoint (e.position);
    if (padDragging)
    {
        if (processorOwnsMotion && onMorphPositionHostWrite)
            onMorphPositionHostWrite (posX, posY, false, false);
        else
            tryEmitDrag();
    }
    repaint();
}

void MorpherPanel::mouseUp (const juce::MouseEvent& e)
{
    if (! padGesture || mode != Mode::morph || padBounds.isEmpty())
    {
        cancelPadGesture();
        return;
    }
    padGesture = false;

    if (lockRefDragging)
    {
        updateLockRefFromPoint (e.position);
        lockRefDragging = false;
        commitLockRef();
        repaint();
        return;
    }

    if (e.mods.isPopupMenu())
        return;

    updatePositionFromPoint (e.position);

    const bool wasDrag = padDragging;
    const bool hadPending = pendingMorph;
    padDragging = false;
    pendingMorph = false;

    // Cancel coalesced drag timer so a late param burst cannot land after the final dump.
    if (! lfoEnabled)
        stopTimer();

    if (wasDrag)
    {
        if (processorOwnsMotion && onMorphPositionHostWrite)
        {
            lastSentVoice.reset();
            onMorphPositionHostWrite (posX, posY, false, true);
        }
        else
        {
            if (hadPending)
            {
                lastMorphEmitMs = juce::Time::getMillisecondCounter();
                emitMorph (true);
            }
            lastSentVoice.reset();
            lastMorphEmitMs = juce::Time::getMillisecondCounter();
            emitMorph (false);
        }
    }
    else if (processorOwnsMotion && onMorphPositionHostWrite)
    {
        onMorphPositionHostWrite (posX, posY, true, true);
    }
    else
    {
        lastMorphEmitMs = juce::Time::getMillisecondCounter();
        emitMorph (false);
    }

    ensureTimerRunning();
    repaint();
}

void MorpherPanel::updatePositionFromPoint (juce::Point<float> p)
{
    posX = juce::jlimit (0.0f, 1.0f, (p.x - (float) padBounds.getX()) / (float) padBounds.getWidth());
    posY = juce::jlimit (0.0f, 1.0f, (p.y - (float) padBounds.getY()) / (float) padBounds.getHeight());
}

void MorpherPanel::advanceLfo (double deltaSeconds)
{
    if (processorOwnsMotion)
        return;
    if (! lfoEnabled || padDragging || lockRefDragging || mode != Mode::morph || ! allCornersReady()
        || egressPaused)
        return;

    lfoPhase += (float) (deltaSeconds * (double) lfoRateHz);
    if (std::abs (lfoRateHz) < kMorphLfoPauseHz)
        return;
    const float wrapped = lfoPhase - std::floor (lfoPhase);
    const int step = juce::jlimit (0, kMorphLfoStepsPerLoop - 1,
                                   (int) std::floor (wrapped * (float) kMorphLfoStepsPerLoop));
    if (step == lastLfoStep)
        return;
    lastLfoStep = step;

    const float qPhase = ((float) step + 0.5f) / (float) kMorphLfoStepsPerLoop;
    morphPadEdgePosition (qPhase, true, posX, posY);
    tryEmitDrag();
    repaint();
}

void MorpherPanel::ensureTimerRunning()
{
    if (lfoEnabled && mode == Mode::morph)
    {
        if (! isTimerRunning() || getTimerInterval() != kLfoTimerMs)
            startTimer (kLfoTimerMs);
        return;
    }
    if (pendingMorph && padDragging)
        return;
    if (! pendingMorph)
        stopTimer();
}

void MorpherPanel::tryEmitDrag()
{
    const auto now = juce::Time::getMillisecondCounter();
    const int interval = effectiveEmitIntervalMs();
    const auto elapsed = now - lastMorphEmitMs;
    if (elapsed < (uint32_t) interval)
    {
        pendingMorph = true;
        // The LFO timer is already running and will flush the pending emit itself.
        if (! (lfoEnabled && mode == Mode::morph) && ! isTimerRunning())
        {
            const int wait = juce::jmax (1, interval - (int) elapsed);
            startTimer (wait);
        }
        return;
    }
    pendingMorph = false;
    if (! (lfoEnabled && mode == Mode::morph))
        stopTimer();
    lastMorphEmitMs = now;
    emitMorph (true);
}

void MorpherPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (lfoEnabled && mode == Mode::morph)
    {
        const double dt = lastTimerMs == 0 ? 0.0 : (double) (now - lastTimerMs) * 0.001;
        lastTimerMs = now;
        advanceLfo (dt);
        if (pendingMorph && ! padDragging)
        {
            const auto elapsed = now - lastMorphEmitMs;
            if (elapsed >= (uint32_t) effectiveEmitIntervalMs())
            {
                pendingMorph = false;
                lastMorphEmitMs = now;
                emitMorph (true);
            }
        }
        return;
    }

    // One-shot coalesced drag emit — only while still dragging.
    stopTimer();
    if (! pendingMorph || ! padDragging)
        return;
    pendingMorph = false;
    lastMorphEmitMs = now;
    emitMorph (true);
}

void MorpherPanel::emitMorph (bool dragEmit, bool liveAllParams)
{
    if (! onMorph || ! allCornersReady() || egressPaused)
        return;

    auto voice = morphAtCurrent();
    if (lastSentVoice.has_value() && *lastSentVoice == voice)
        return;

    lastSentVoice = voice;
    onMorph (voice, dragEmit, liveAllParams);
}

void MorpherPanel::refreshPresetList()
{
    presetList.updateContent();
    presetList.repaint();
}

void MorpherPanel::saveCurrentPreset()
{
    if (store == nullptr)
        return;
    auto name = presetName.getText().trim();
    if (name.isEmpty())
    {
        if (allCornersReady())
            name = juce::String::fromUTF8 (voiceNameFromData (morphAtCurrent()).c_str());
        else
            name = "Morph set";
    }
    store->add (currentAsPreset (name));
    if (onPresetsChanged)
        onPresetsChanged();
    refreshPresetList();
}

void MorpherPanel::loadSelectedPreset (bool switchToMorphPad)
{
    if (store == nullptr)
        return;
    const int row = presetList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;
    const auto& p = store->items()[static_cast<size_t> (row)];

    corners[0] = p.a;
    corners[1] = p.b;
    corners[2] = p.c;
    corners[3] = p.d;
    cornerSet[0] = cornerSet[1] = cornerSet[2] = cornerSet[3] = true;
    cornerNames[0] = juce::String::fromUTF8 (p.nameA.c_str());
    cornerNames[1] = juce::String::fromUTF8 (p.nameB.c_str());
    cornerNames[2] = juce::String::fromUTF8 (p.nameC.c_str());
    cornerNames[3] = juce::String::fromUTF8 (p.nameD.c_str());
    posX = juce::jlimit (0.0f, 1.0f, p.posX);
    posY = juce::jlimit (0.0f, 1.0f, p.posY);
    lockGroups = p.lockGroups & morphLockAllGroups;
    lockRefX = juce::jlimit (0.0f, 1.0f, p.lockRefX);
    lockRefY = juce::jlimit (0.0f, 1.0f, p.lockRefY);
    syncLockChipsFromFlags();
    lastSentVoice.reset();
    lastLfoStep = -1;
    lastTimerMs = juce::Time::getMillisecondCounter();
    if (onMorphInvalidate)
        onMorphInvalidate();
    egressPaused = false;
    updateHint();
    updateMorphControlsEnabled();
    emitMorph (false);
    ensureTimerRunning();
    repaint();

    if (onPresetApplied)
        onPresetApplied();

    if (switchToMorphPad)
        setMode (Mode::morph);
}

void MorpherPanel::deleteSelectedPreset()
{
    if (store == nullptr)
        return;
    suppressPresetAutoLoad = true;
    store->removeAt (presetList.getSelectedRow());
    if (onPresetsChanged)
        onPresetsChanged();
    refreshPresetList();
    suppressPresetAutoLoad = false;
}

void MorpherPanel::saveSelectedPresetToDeviceSlot()
{
    if (! onSaveToDeviceSlot || store == nullptr)
        return;
    const int row = presetList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;
    const auto& p = store->items()[static_cast<size_t> (row)];
    const auto voice = VoiceMorpher::morph4 (p.a, p.b, p.c, p.d, p.posX, p.posY,
                                             p.lockGroups, p.lockRefX, p.lockRefY);
    const int slot = deviceSlot.getSelectedId();
    if (slot < 1 || slot > 32)
        return;
    onSaveToDeviceSlot (voice, slot);
}

int MorpherPanel::getNumRows()
{
    return store != nullptr ? static_cast<int> (store->items().size()) : 0;
}

void MorpherPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (store == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;
    g.fillAll (selected ? findColour (juce::TextEditor::highlightColourId).withAlpha (0.45f)
                         : findColour (juce::ListBox::backgroundColourId));
    g.setColour (findColour (juce::Label::textColourId));
    const auto& p = store->items()[static_cast<size_t> (row)];
    auto label = juce::String::fromUTF8 (p.name.c_str())
               + "  (" + juce::String (p.posX, 2) + ", " + juce::String (p.posY, 2) + ")";
    g.drawText (label, 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void MorpherPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        presetList.selectRow (row);
        showPresetContextMenu (row);
        return;
    }
}

void MorpherPanel::showPresetContextMenu (int row)
{
    if (store == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;

    enum { openFolder = 1 };
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addItem (openFolder, "Open in folder");

    juce::Component::SafePointer<MorpherPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safe] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            if (result == 1 && safe->onRevealMorphPresetsFile)
                                safe->onRevealMorphPresetsFile();
                        });
}

void MorpherPanel::selectedRowsChanged (int)
{
    if (suppressPresetAutoLoad || mode != Mode::presets)
        return;
    if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu())
        return;
    loadSelectedPreset (false);
}

void MorpherPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    loadSelectedPreset (true);
}

} // namespace fmlib
