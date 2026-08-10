#pragma once

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
#include <set>
#include <thread>

class FmLibPlugAudioProcessor : public juce::AudioProcessor
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
    void handleIncomingSysex (const std::vector<uint8_t>& bytes);
    void sendVoice (const fmlib::VoiceData& voice);
    /** Immediate audition using prefs note/velocity/duration. */
    void auditionNote();
    /**
     * Audition after delayMs (lets morph SysEx settle). Uses prefs note/vel/duration.
     * delayMs <= 0 plays immediately.
     */
    void auditionNoteAfterDelay (int delayMs);
    /**
     * Audition-style mono note: optional delay before note-on; replaces any prior audition note.
     * Prefer playControllerNote for polyphonic controller performance.
     */
    void playHeldNoteAfterDelay (int note, int velocity, int delayMs);
    /** Polyphonic controller note-on/off (does not steal other held notes). */
    void playControllerNote (int note, int velocity);
    /** Like playControllerNote but waits delayMs so morph SysEx can land first.
     *  Further calls while a lead-in is pending join the same delayed chord batch. */
    void playControllerNoteAfterDelay (int note, int velocity, int delayMs);
    void releaseControllerNote (int note);
    void releaseAllControllerNotes();
    /** True while a delayed controller note-on batch is waiting (not yet sounding). */
    bool hasPendingControllerLeadIn() const;
    /** True while audition or controller notes are actually sounding on the wire. */
    bool hasSoundingNotes() const;
    void autoTagLibrary (std::function<void()> onDone = {});
    juce::File tagsFile() const;
    juce::File morphPresetsFile() const;

private:
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

    int lastAuditionNote = -1;
    int pendingNote = -1;
    int pendingVelocity = 0;
    bool pendingAutoOff = false;
    int pendingAutoOffMs = 250;
    int pendingControllerNote = -1;
    int pendingControllerVelocity = 0;
    /** Notes queued to fire with the current lead-in (first note is also pendingController*). */
    std::vector<std::pair<int, int>> pendingControllerChord;
    std::set<int> controllerSoundingNotes;
    std::thread autoTagWorker;
    std::atomic<bool> autoTagRunning { false };
    std::atomic<bool> autoTagCancel { false };
    std::atomic<uint64_t> autoTagEpoch { 0 };
    /**
     * Cleared first thing in the destructor. Async callbacks capture it by value, so the
     * flag outlives the processor and they can test it without touching a dead `this`.
     * Both the check and the reset happen on the message thread, so there is no race.
     */
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>> (true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmLibPlugAudioProcessor)
};
