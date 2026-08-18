#pragma once

#include "host/MorphHostState.h"
#include "host/PluginSessionState.h"
#include "library/DeviceBuffer.h"
#include "library/FavoritesStore.h"
#include "library/MorphPresetStore.h"
#include "library/PatchLibrary.h"
#include "library/RecentStore.h"
#include "library/TagStore.h"
#include "midi/MidiDeviceManager.h"
#include "prefs/AppPreferences.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <thread>

class FmLibPlugAudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
public:
    FmLibPlugAudioProcessor();
    ~FmLibPlugAudioProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    fmlib::AppPreferences prefs;
    fmlib::PatchLibrary library;
    fmlib::DeviceBuffer deviceBuffer;
    fmlib::FavoritesStore favorites;
    fmlib::TagStore tags;
    fmlib::RecentStore recent;
    fmlib::MorphPresetStore morphPresets;
    fmlib::MidiDeviceManager midi;

    void applyPreferencesToEngine();
    void persistPreferences();
    void persistTags();
    void persistMorphPresets();
    void persistAllUserData();
    void handleIncomingSysex (const std::vector<uint8_t>& bytes);
    void sendVoice (const fmlib::VoiceData& voice);
    void rememberEditBufferVoice (const fmlib::VoiceData& voice);
    std::optional<fmlib::VoiceData> getLastEditBufferVoice() const { return lastEditBufferVoice; }
    void auditionNote();
    void auditionNoteAfterDelay (int delayMs);
    void auditionHoldStart (int delayMs);
    void auditionHoldStop();
    void playHeldNoteAfterDelay (int note, int velocity, int delayMs);
    void playControllerNote (int note, int velocity);
    void playControllerNoteAfterDelay (int note, int velocity, int delayMs);
    void releaseControllerNote (int note);
    void releaseAllControllerNotes();
    bool hasControllerNotesHeld() const { return ! controllerHeldNotes.empty(); }
    bool hasPendingControllerLeadIn() const;
    bool hasSoundingNotes() const;
    void autoTagLibrary (std::function<void()> onDone = {});
    juce::File tagsFile() const;
    juce::File morphPresetsFile() const;

    /** Live morph snapshot owned by the processor (corners, locks, names). */
    const fmlib::MorphPreset& getLiveMorph() const { return liveMorph; }
    bool isLiveCornerSet (int corner0to3) const;
    bool allLiveCornersReady() const;
    void setLiveCorner (int corner0to3, const fmlib::VoiceData& v, const juce::String& name);
    void clearLiveCorners();
    void applyLiveMorphPreset (const fmlib::MorphPreset& p);
    fmlib::MorphPreset buildLiveMorphSnapshot() const;
    void setLiveLockGroups (uint32_t groups);
    uint32_t getLiveLockGroups() const { return liveMorph.lockGroups; }

    void setMorphEgressPaused (bool paused);
    bool isMorphEgressPaused() const { return morphEgressPaused; }

    float getMorphX() const;
    float getMorphY() const;
    float getLockRefX() const;
    float getLockRefY() const;
    float getMorphLfoRate() const;
    bool getMorphLfoSync() const;
    int getMorphLfoDivision() const;
    int getMorphMotionChoice() const;

    void setMorphPosition (float x, float y, bool beginGesture, bool endGesture);
    void setLockRefPosition (float x, float y, bool beginGesture, bool endGesture);
    void setMorphLfoRateParam (float hz, bool notifyHost);
    void setMorphLfoSyncParam (bool sync, bool notifyHost);
    void setMorphLfoDivisionParam (int division, bool notifyHost);
    void setMorphMotionChoice (int choice, bool notifyHost);
    void applyMorphMotionMode (int choice);

    void applyLiveMorph (bool dragEmit, bool liveAllParams);
    bool applyNoteJump();
    void advanceMorphLfo (double deltaSeconds);
    void notifyMorphUiSync();

    /** Editor registers to receive param/snapshot sync callbacks. */
    void setMorphUiSyncCallback (std::function<void()> fn) { morphUiSync = std::move (fn); }

    bool showDawMidiPorts() const;

    int lastEditorPage = 0; // 0 library, 1 settings, 2 morph
    int lastMorpherPresetsPage = 0;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void bindMorphParameters();
    void restoreSessionFromState (const fmlib::PluginSessionState& state, bool notifyUi);
    void pushLiveMorphToApvts();
    void publishMorphMotionPosition (float x, float y, bool dragEmit);
    void handleMorphPositionHostChange (const juce::String& parameterID, float newValue);
    void requestApplyLiveMorph (bool dragEmit, bool liveAllParams);
    void chaseHostMorphParameters();
    void cacheHostTransport();
    bool isTransportPlaying() const;
    float effectiveMorphLfoRateHz() const;
    void updateMorphMotionEngine();
    int morphNoteLeadMs() const;
    void handleControllerNoteOn (int note, int velocity);
    void handleControllerNoteOff (int note);

    class AuditionOffTimer : public juce::Timer
    {
    public:
        FmLibPlugAudioProcessor* owner = nullptr;
        void timerCallback() override;
    } auditionOff;

    class AuditionOnTimer : public juce::Timer
    {
    public:
        FmLibPlugAudioProcessor* owner = nullptr;
        void timerCallback() override;
    } auditionOn;

    class ControllerNoteDelayTimer : public juce::Timer
    {
    public:
        FmLibPlugAudioProcessor* owner = nullptr;
        void timerCallback() override;
    } controllerNoteDelay;

    fmlib::MorphPreset liveMorph;
    bool liveCornerSet[4] { false, false, false, false };
    bool morphEgressPaused = false;
    std::atomic<int> morphSelfWriteDepth { 0 };
    float morphLastWrittenX = -1.0f;
    float morphLastWrittenY = -1.0f;
    float morphLastAppliedX = -1.0f;
    float morphLastAppliedY = -1.0f;
    std::atomic<bool> morphApplyPending { false };
    std::atomic<bool> morphApplyLiveAllPending { false };
    std::atomic<bool> morphUiSyncPending { false };
    std::atomic<bool> morphUiSyncDeferred { false };
    std::atomic<uint32_t> morphLastApplyMs { 0 };
    std::atomic<uint32_t> morphLastUiSyncMs { 0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostPpq { 0.0 };
    std::atomic<bool> hostPlaying { false };
    int morphEdgeJumpIndex = 0;
    float morphLfoPhase = 0.0f;
    int morphLastLfoStep = -1;
    uint32_t morphLastLfoTimerMs = 0;
    std::set<int> controllerHeldNotes;
    std::function<void()> morphUiSync;

    int lastAuditionNote = -1;
    int pendingNote = -1;
    int pendingVelocity = 0;
    bool pendingAutoOff = false;
    int pendingAutoOffMs = 250;
    bool auditionHoldActive = false;
    std::optional<fmlib::VoiceData> lastEditBufferVoice;
    int pendingControllerNote = -1;
    int pendingControllerVelocity = 0;
    std::vector<std::pair<int, int>> pendingControllerChord;
    std::set<int> controllerSoundingNotes;
    std::thread autoTagWorker;
    std::atomic<bool> autoTagRunning { false };
    std::atomic<bool> autoTagCancel { false };
    std::atomic<uint64_t> autoTagEpoch { 0 };
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>> (true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmLibPlugAudioProcessor)
};
