#pragma once

#include "midi/MorphTransport.h"
#include "sysex/Dx7Formats.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace fmlib
{

class MidiDeviceManager : private juce::MidiInputCallback,
                          private juce::Timer
{
public:
    using StatusFn = std::function<void(const juce::String&)>;
    using SysexFn = std::function<void(const std::vector<uint8_t>&)>;
    using NoteOnFn = std::function<void(int note, int velocity)>;
    using NotesSoundingFn = std::function<bool()>;

    MidiDeviceManager();
    ~MidiDeviceManager() override;

    void refreshDevices();
    juce::StringArray getInputNames (bool includeDaw = false) const;
    juce::StringArray getOutputNames (bool includeDaw = false) const;

    bool openInputByName (const juce::String& name);
    bool openControllerInputByName (const juce::String& name);
    bool openOutputByName (const juce::String& name);
    void close();

    bool usesHostDeviceInput() const { return hostDeviceInput; }
    bool usesHostControllerInput() const { return hostControllerInput; }
    bool usesHostOutput() const { return hostOutput; }

    /**
     * Audio thread: consume host MIDI for the (DAW) ports, then replace the buffer
     * with plugin-originated output only. Incoming notes/CC never pass through;
     * they reach MIDI out only via controller thru or sendNoteOn / SysEx.
     */
    void exchangeHostMidi (juce::MidiBuffer& midiMessages);
    /** Audio thread: consume host MIDI in; posts callbacks on the message thread. */
    void processHostMidiInput (const juce::MidiBuffer& midiMessages);
    /** Audio thread: append queued host MIDI out events. */
    void collectHostMidiOutput (juce::MidiBuffer& midiMessages);

    void setChannel (int channel1to16);
    int getChannel() const { return channel; }

    void setSysexPacingMs (int ms) { pacingMs = juce::jmax (0, ms); }
    int getSysexPacingMs() const { return pacingMs; }

    /**
     * How long morph SysEx is held after the last note-off. A voice dump re-latches the
     * whole patch, so one landing in the release tail of the note you just let go clicks.
     * 0 disables the hold.
     */
    void setMorphReleaseGuardMs (int ms) { morphReleaseGuardMs = juce::jlimit (0, 2000, ms); }

    void setControllerThru (bool enabled) { controllerThru.store (enabled, std::memory_order_relaxed); }
    /** When true, note on/off go to callbacks only (not thru); CC/other still thru if enabled. */
    void setControllerNotesToCallbacksOnly (bool enabled)
    {
        controllerNotesToCallbacksOnly.store (enabled, std::memory_order_relaxed);
    }

    void setMorphStreamMode (MorphStreamMode m);
    void setMorphByteBudget (int bytes);
    /** Query used each morph tick to gate SysEx (audition + controller + thru). */
    void setNotesSoundingQuery (NotesSoundingFn fn) { notesSoundingFn = std::move (fn); }
    /** Push latest sounding state into the transport (call on note on/off). */
    void syncNotesSounding (bool sounding);
    /** True while controller-thru notes are held (morph Off + Forward MIDI). */
    bool hasThruNotesSounding() const;
    /** Milliseconds until the scheduled SysEx queue is expected idle (0 if free). */
    int getMorphWireBusyRemainingMs() const;
    /** As above, plus the morph target still queued for the wire. Use to time a delayed note-on. */
    int getMorphLeadEstimateMs() const;
    /**
     * Drop the post-note-off hold on morph egress. Call before a note jump: the
     * pending note-on masks any release-tail click, and its dump must not wait.
     */
    void cancelMorphReleaseGuard();

    bool sendRaw (const std::vector<uint8_t>& bytes, bool pace = true);
    bool sendVoice (const VoiceData& voice, bool pace = true);
    /**
     * Queue a morph target. forceCommit: full dump preferred when idle (click / drag-end / jump).
     * liveAllParams: stream non-freq changes while notes are held (lock-reference).
     * Transport ticks flush under the active stream mode + byte budget.
     */
    bool sendMorphVoice (const VoiceData& voice, bool forceCommit = false, bool liveAllParams = false);
    void invalidateMorphBaseline();
    bool sendBank (const std::array<VoiceData, kBankVoiceCount>& bank);
    bool requestDump (bool bank32);
    bool sendNoteOn (int note, int velocity);
    bool sendNoteOff (int note);

    void setStatusCallback (StatusFn fn) { statusFn = std::move (fn); }
    void setSysexReceivedCallback (SysexFn fn) { sysexFn = std::move (fn); }
    void setControllerNoteOnCallback (NoteOnFn fn) { noteOnFn = std::move (fn); }
    void setControllerNoteOffCallback (NoteOnFn fn) { noteOffFn = std::move (fn); }

    juce::String getInputName() const { return inputName; }
    juce::String getControllerInputName() const { return controllerInputName; }
    juce::String getOutputName() const { return outputName; }


private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;
    void timerCallback() override;
    void setStatus (const juce::String& s);
    void ensureBackgroundThread();
    bool hasOutput() const;
    bool sendMessagesScheduled (const std::vector<std::vector<uint8_t>>& messages, int spacingMs);
    bool sendMessageNowLocked (const juce::MidiMessage& message);
    void enqueueHostOutput (const juce::MidiMessage& message, double dueMs = 0.0);
    void handleControllerMessage (const juce::MidiMessage& message, bool thruAlreadyHandled = false);
    void thruControllerMessageIfEnabled (const juce::MidiMessage& message);
    void handleDeviceSysex (const juce::MidiMessage& message);
    void ensureMorphTimer();
    void clearThruHeldNotes();
    void setThruHeldNoteBit (int note, bool held);
    void flushThruRing();
    void requestThruFlush();
    void discardThruRing();
    void updateNotesSounding (bool sounding);
    bool inMorphReleaseGuard() const;
    static int estimateSysexWireMs (int byteCount);

    /** Short MIDI events deferred from audio/MIDI callback to the message thread (hardware out). */
    struct ThruPod
    {
        uint8_t data[3] {};
        uint8_t size = 0; // 1..3
    };

    static constexpr int kThruRingSize = 512;

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiInput> controllerInput;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String inputName, controllerInputName, outputName;
    bool hostDeviceInput = false;
    bool hostControllerInput = false;
    bool hostOutput = false;
    juce::CriticalSection hostOutLock;
    struct HostOutEvent
    {
        juce::MidiMessage message;
        double dueMs = 0.0;
    };
    std::vector<HostOutEvent> hostOutQueue;
    int channel = 1;
    int pacingMs = 20;
    int morphReleaseGuardMs = 250;
    std::atomic<bool> controllerThru { false };
    std::atomic<bool> controllerNotesToCallbacksOnly { false };
    bool bgThreadStarted = false;
    juce::CriticalSection outputLock;

    juce::AbstractFifo thruFifo { kThruRingSize };
    std::array<ThruPod, kThruRingSize> thruRing {};
    /** True while a message-thread flush is queued or running. */
    std::atomic<bool> thruFlushQueued { false };

    MorphTransport morph;
    NotesSoundingFn notesSoundingFn;
    /** Note 0..63 in lo, 64..127 in hi — lock-free for audio-thread thru. */
    std::atomic<uint64_t> thruHeldLo { 0 };
    std::atomic<uint64_t> thruHeldHi { 0 };
    double morphBusyUntilMs = 0.0;
    double notesQuietAtMs = 0.0;
    bool notesSoundingLatched = false;
    /** Cleared in the destructor; captured by value so queued lambdas never touch a dead `this`. */
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>> (true) };
    std::atomic<uint64_t> thruSyncEpoch { 0 };

    juce::String status { "No MIDI devices selected" };
    StatusFn statusFn;
    SysexFn sysexFn;
    NoteOnFn noteOnFn;
    NoteOnFn noteOffFn;
};

} // namespace fmlib
