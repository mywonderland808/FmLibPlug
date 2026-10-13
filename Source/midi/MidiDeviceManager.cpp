#include "midi/MidiDeviceManager.h"
#include "midi/MidiPortNames.h"
#include "sysex/SysexMessages.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace fmlib
{

namespace
{
constexpr int kMorphTickMs = 20;
constexpr int kHostPumpMs = 8;

inline bool isNoteOnRaw (const uint8_t* raw, int rawSize)
{
    return raw != nullptr && rawSize > 2 && (raw[0] & 0xf0) == 0x90 && raw[2] > 0;
}

inline bool isNoteOffRaw (const uint8_t* raw, int rawSize)
{
    if (raw == nullptr || rawSize < 2)
        return false;
    const auto st = raw[0] & 0xf0;
    return st == 0x80 || (st == 0x90 && rawSize > 2 && raw[2] == 0);
}

inline int noteNumberRaw (const uint8_t* raw, int rawSize)
{
    return rawSize > 1 ? (int) (raw[1] & 0x7f) : 0;
}
} // namespace

int MidiDeviceManager::estimateSysexWireMs (int byteCount)
{
    // MIDI 1.0 SysEx streams at ~3125 bytes/s (~0.32 ms/byte).
    return juce::jmax (1, (juce::jmax (0, byteCount) + 2) / 3);
}

MidiDeviceManager::MidiDeviceManager() = default;

MidiDeviceManager::~MidiDeviceManager()
{
    *alive = false;
    close();
}

void MidiDeviceManager::setStatus (const juce::String& s)
{
    status = s;
    if (statusFn)
        statusFn (status);
}

void MidiDeviceManager::refreshDevices()
{
    setStatus ("MIDI device lists refreshed");
}

juce::StringArray MidiDeviceManager::getInputNames (bool includeDaw) const
{
    juce::StringArray names;
    if (includeDaw)
        names.add (kDawMidiPortName);
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add (d.name);
    return names;
}

juce::StringArray MidiDeviceManager::getOutputNames (bool includeDaw) const
{
    juce::StringArray names;
    if (includeDaw)
        names.add (kDawMidiPortName);
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        names.add (d.name);
    return names;
}

bool MidiDeviceManager::openInputByName (const juce::String& name)
{
    input.reset();
    inputName.clear();
    hostDeviceInput = false;
    if (name.isEmpty())
    {
        setStatus ("MIDI device input closed");
        return true;
    }
    if (isDawMidiPortName (name))
    {
        hostDeviceInput = true;
        inputName = name;
        ensureHostPumpTimer();
        setStatus ("MIDI in: " + name);
        return true;
    }
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (d.name == name)
        {
            input = juce::MidiInput::openDevice (d.identifier, this);
            if (input)
            {
                input->start();
                inputName = name;
                setStatus ("MIDI in: " + name);
                return true;
            }
        }
    }
    setStatus ("Failed to open MIDI input: " + name);
    return false;
}

bool MidiDeviceManager::openControllerInputByName (const juce::String& name)
{
    if (controllerInput != nullptr)
        controllerInput->stop();
    controllerInput.reset();
    controllerInputName.clear();
    hostControllerInput = false;
    clearThruHeldNotes();
    discardThruRing();
    if (name.isEmpty())
    {
        setStatus ("MIDI controller input closed");
        syncNotesSounding (notesSoundingFn ? notesSoundingFn() : false);
        return true;
    }
    if (isDawMidiPortName (name))
    {
        hostControllerInput = true;
        controllerInputName = name;
        ensureHostPumpTimer();
        setStatus ("MIDI controller in: " + name);
        return true;
    }
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (d.name == name)
        {
            controllerInput = juce::MidiInput::openDevice (d.identifier, this);
            if (controllerInput)
            {
                controllerInput->start();
                controllerInputName = name;
                setStatus ("MIDI controller in: " + name);
                return true;
            }
        }
    }
    setStatus ("Failed to open MIDI controller input: " + name);
    return false;
}

void MidiDeviceManager::clearHostRingsSafe()
{
    // Stop audio producers/consumers before AbstractFifo::reset (not SPSC-safe vs live R/W).
    hostMidiSuspended.store (true, std::memory_order_release);
    hostOutput.store (false, std::memory_order_release);

    // Never reset while audio is inside exchangeHostMidi. If depth stays >0 (stuck callback),
    // keep waiting — do not clear — but surface diagnostics after a few seconds.
    bool warnedStuck = false;
    const auto waitStart = std::chrono::steady_clock::now();
    for (int i = 0; audioHostMidiDepth.load (std::memory_order_acquire) > 0; ++i)
    {
        std::this_thread::yield();
        if ((i & 63) == 63)
            std::this_thread::sleep_for (std::chrono::microseconds (50));
        else if (i > 10'000)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));

        if (! warnedStuck
            && std::chrono::steady_clock::now() - waitStart > std::chrono::seconds (3))
        {
            warnedStuck = true;
            jassertfalse; // audioHostMidiDepth stuck >0 while clearing host rings
            setStatus ("MIDI: waiting for audio thread before host ring reset");
        }
    }

    {
        std::lock_guard<std::mutex> lock (hostOutProduceMutex);
        hostOutShort.clear();
        hostOutSysex.clear();
        hostInRing.clear();
    }
    hostThruScratchCount = 0;
    hostInSysexOpen.store (false, std::memory_order_relaxed);
    hostInSysexAbort.store (false, std::memory_order_relaxed);
    hostSysexReassembly.clear();
    hostMidiSuspended.store (false, std::memory_order_release);
}

bool MidiDeviceManager::openOutputByName (const juce::String& name)
{
    // Stop the thru consumer before tearing down MidiOutput / resetting the FIFO.
    discardThruRing();
    {
        const juce::ScopedLock sl (outputLock);
        if (output && bgThreadStarted)
            output->stopBackgroundThread();
        output.reset();
        bgThreadStarted = false;
        outputName.clear();
    }
    clearHostRingsSafe();
    discardThruRing();

    if (name.isEmpty())
    {
        setStatus ("MIDI output closed");
        return true;
    }
    if (isDawMidiPortName (name))
    {
        outputName = name;
        hostOutput.store (true, std::memory_order_release);
        ensureHostPumpTimer();
        setStatus ("MIDI out: " + name);
        return true;
    }

    for (auto& d : juce::MidiOutput::getAvailableDevices())
    {
        if (d.name == name)
        {
            auto out = juce::MidiOutput::openDevice (d.identifier);
            if (out)
            {
                const juce::ScopedLock sl (outputLock);
                output = std::move (out);
                outputName = name;
                ensureBackgroundThread();
                // Start thru worker off the audio thread (never from requestThruFlush).
                ensureThruFlushThread();
                setStatus ("MIDI out: " + name);
                return true;
            }
        }
    }
    setStatus ("Failed to open MIDI output: " + name);
    return false;
}

void MidiDeviceManager::close()
{
    ++thruSyncEpoch; // cancel pending thru sync lambdas
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        stopTimer();
    sysexFn = nullptr;
    noteOnFn = nullptr;
    noteOffFn = nullptr;
    notesSoundingFn = nullptr;
    // Stop MIDI inputs before joining the thru worker so callbacks cannot re-arm flush.
    if (input)
        input->stop();
    if (controllerInput)
        controllerInput->stop();
    input.reset();
    controllerInput.reset();
    clearThruHeldNotes();
    discardThruRing();
    {
        const juce::ScopedLock sl (outputLock);
        if (output && bgThreadStarted)
            output->stopBackgroundThread();
        output.reset();
        bgThreadStarted = false;
    }
    inputName.clear();
    controllerInputName.clear();
    outputName.clear();
    hostDeviceInput = false;
    hostControllerInput = false;
    messagePumpFn = nullptr;
    clearHostRingsSafe();
    morphBusyUntilMs = 0.0;
    notesQuietAtMs = 0.0;
    notesSoundingLatched = false;
}

void MidiDeviceManager::clearThruHeldNotes()
{
    thruHeldLo.store (0, std::memory_order_relaxed);
    thruHeldHi.store (0, std::memory_order_relaxed);
}

void MidiDeviceManager::setThruHeldNoteBit (int note, bool held)
{
    const int n = juce::jlimit (0, 127, note);
    auto& word = (n < 64) ? thruHeldLo : thruHeldHi;
    const uint64_t mask = uint64_t (1) << (n & 63);
    if (held)
        word.fetch_or (mask, std::memory_order_relaxed);
    else
        word.fetch_and (~mask, std::memory_order_relaxed);
}

bool MidiDeviceManager::hasThruNotesSounding() const
{
    return thruHeldLo.load (std::memory_order_relaxed) != 0
        || thruHeldHi.load (std::memory_order_relaxed) != 0;
}

void MidiDeviceManager::discardThruRing()
{
    stopThruFlushThread();
    thruRing.clear();
    thruFlushQueued.store (false, std::memory_order_relaxed);
}

void MidiDeviceManager::ensureThruFlushThread()
{
    const juce::ScopedLock sl (thruThreadLock);
    if (thruFlushThread.joinable())
        return;

    thruStop.store (false, std::memory_order_release);
    thruFlushThread = std::thread ([this, aliveFlag = alive]
    {
        while (! thruStop.load (std::memory_order_acquire) && aliveFlag->load())
        {
            // Timed wait: audio must not call WaitableEvent::signal (mutex/condvar).
            thruWake.wait (1);
            if (thruStop.load (std::memory_order_acquire) || ! aliveFlag->load())
                break;
            if (thruFlushQueued.load (std::memory_order_acquire))
                flushThruRing();
        }
    });
}

void MidiDeviceManager::stopThruFlushThread()
{
    {
        const juce::ScopedLock sl (thruThreadLock);
        thruStop.store (true, std::memory_order_release);
    }
    thruWake.signal();

    std::thread local;
    {
        const juce::ScopedLock sl (thruThreadLock);
        if (thruFlushThread.joinable())
            local = std::move (thruFlushThread);
    }
    if (local.joinable())
        local.join();
}

void MidiDeviceManager::requestThruFlush()
{
    // Worker polls (~1 ms). Do not signal WaitableEvent from the audio thread.
    bool expected = false;
    thruFlushQueued.compare_exchange_strong (expected, true, std::memory_order_acq_rel);
}

void MidiDeviceManager::flushThruRing()
{
    ThruShortRing::Pod pod;
    bool sentAny = false;
    while (thruRing.pop (pod))
    {
        if (pod.size > 0 && pod.size <= 3)
            sendMessageNowLocked (juce::MidiMessage (pod.data, (int) pod.size));
        sentAny = true;
    }

    thruFlushQueued.store (false, std::memory_order_release);
    if (thruRing.numReady() > 0)
    {
        bool expected = false;
        if (thruFlushQueued.compare_exchange_strong (expected, true, std::memory_order_acq_rel))
            thruWake.signal();
    }
    juce::ignoreUnused (sentAny);
}

int MidiDeviceManager::getMorphWireBusyRemainingMs() const
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now >= morphBusyUntilMs)
        return 0;
    return (int) std::ceil (morphBusyUntilMs - now);
}

int MidiDeviceManager::getMorphLeadEstimateMs() const
{
    int ms = getMorphWireBusyRemainingMs();
    // A target queued behind busy wire has not been scheduled yet: allow the tick that
    // will pick it up plus the dump it produces, or the note-on overtakes the new voice.
    if (morph.pendingDiffCount() > 0 || (morph.hasTarget() && ! morph.hasBaseline()))
        ms += kMorphTickMs + estimateSysexWireMs (MorphTransport::kVoiceDumpBytes);
    return ms;
}

void MidiDeviceManager::cancelMorphReleaseGuard()
{
    notesQuietAtMs = 0.0;
}

void MidiDeviceManager::updateNotesSounding (bool sounding)
{
    if (notesSoundingLatched && ! sounding)
        notesQuietAtMs = juce::Time::getMillisecondCounterHiRes();
    notesSoundingLatched = sounding;
    morph.setNotesSounding (sounding);
}

bool MidiDeviceManager::inMorphReleaseGuard() const
{
    if (notesSoundingLatched || notesQuietAtMs <= 0.0 || morphReleaseGuardMs <= 0)
        return false;
    return juce::Time::getMillisecondCounterHiRes() < notesQuietAtMs + morphReleaseGuardMs;
}

void MidiDeviceManager::setChannel (int channel1to16)
{
    channel = juce::jlimit (1, 16, channel1to16);
    morph.setChannel (channel);
}

void MidiDeviceManager::setMorphStreamMode (MorphStreamMode m)
{
    morph.setStreamMode (m);
    ensureHostPumpTimer();
}

void MidiDeviceManager::setMorphByteBudget (int bytes)
{
    morph.setByteBudget (bytes);
}

void MidiDeviceManager::syncNotesSounding (bool sounding)
{
    // Thru held notes are OR'd here so MorphTransport is only written from the message thread.
    updateNotesSounding (sounding || hasThruNotesSounding());
    ensureHostPumpTimer();
}

void MidiDeviceManager::ensureBackgroundThread()
{
    if (output != nullptr && ! bgThreadStarted)
    {
        output->startBackgroundThread();
        bgThreadStarted = true;
    }
}

void MidiDeviceManager::ensureHostPumpTimer()
{
    if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
        return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool needMorph = morph.pendingDiffCount() > 0
                        || (morph.hasTarget() && ! morph.hasBaseline())
                        || now < morphBusyUntilMs;
    const bool needIn = hostInRing.numReady() > 0;
    const bool needHostPorts = hostDeviceInput || hostControllerInput
                            || hostOutput.load (std::memory_order_relaxed);
    const bool needPumpFn = static_cast<bool> (messagePumpFn);
    if ((needMorph || needIn || needHostPorts || needPumpFn) && ! isTimerRunning())
        startTimer (kHostPumpMs);
}

void MidiDeviceManager::setMessageThreadPump (MessagePumpFn fn)
{
    messagePumpFn = std::move (fn);
    ensureHostPumpTimer();
}

bool MidiDeviceManager::testPushDeviceSysex (const uint8_t* data, int size, bool fragment)
{
    if (! hostDeviceInput || hostMidiSuspended.load (std::memory_order_acquire))
        return false;
    const bool ok = hostInRing.tryPushSysex (data, size, 0, 0.0, fragment);
    if (ok)
        hostInSysexOpen.store (fragment, std::memory_order_relaxed);
    else
    {
        flagHostOverflow();
        if (fragment || hostInSysexOpen.load (std::memory_order_relaxed))
        {
            hostInSysexOpen.store (false, std::memory_order_relaxed);
            hostInSysexAbort.store (true, std::memory_order_release);
        }
    }
    return ok;
}

void MidiDeviceManager::flagHostOverflow()
{
    hostOverflowPending.store (true, std::memory_order_relaxed);
}

void MidiDeviceManager::reportHostOverflowStatus()
{
    const uint64_t inSyx = hostInRing.droppedSysex();
    const uint64_t outSyx = hostOutSysex.droppedSysex();
    const uint64_t inEv = hostInRing.droppedEvents();
    const bool flagged = hostOverflowPending.exchange (false, std::memory_order_acq_rel);
    const bool newDrops = inSyx > lastReportedInDroppedSysex
                       || outSyx > lastReportedOutDroppedSysex
                       || inEv > lastReportedInDroppedEvents;
    if (! flagged && ! newDrops)
        return;
    lastReportedInDroppedSysex = inSyx;
    lastReportedOutDroppedSysex = outSyx;
    lastReportedInDroppedEvents = inEv;
    setStatus ("Host SysEx dropped — use hardware port");
}

bool MidiDeviceManager::hasOutput() const
{
    return output != nullptr || hostOutput.load (std::memory_order_relaxed);
}

bool MidiDeviceManager::hasDeviceInput() const
{
    return input != nullptr || hostDeviceInput;
}

bool MidiDeviceManager::enqueueHostShort (const uint8_t* data, int size, int sampleOffset)
{
    std::lock_guard<std::mutex> lock (hostOutProduceMutex);
    return hostOutShort.tryPush (data, size, sampleOffset);
}

bool MidiDeviceManager::enqueueHostSysex (const uint8_t* data, int size, double dueMs)
{
    std::lock_guard<std::mutex> lock (hostOutProduceMutex);
    const bool ok = hostOutSysex.tryPushSysex (data, size, 0, dueMs, false);
    if (! ok)
        reportHostOverflowStatus();
    return ok;
}

void MidiDeviceManager::exchangeHostMidi (juce::MidiBuffer& midiMessages)
{
    audioHostMidiDepth.fetch_add (1, std::memory_order_acq_rel);
    hostThruScratchCount = 0;
    processHostMidiInput (midiMessages);
    midiMessages.clear();
    collectHostMidiOutput (midiMessages);
    audioHostMidiDepth.fetch_sub (1, std::memory_order_acq_rel);
}

void MidiDeviceManager::collectHostMidiOutput (juce::MidiBuffer& midiMessages)
{
    if (! hostOutput.load (std::memory_order_acquire))
        return;

    HostShortOutRing::Pod shortPod;
    while (hostOutShort.pop (shortPod))
        midiMessages.addEvent (shortPod.data, (int) shortPod.size, shortPod.sampleOffset);

    for (int i = 0; i < hostThruScratchCount; ++i)
    {
        const auto& p = hostThruScratch[(size_t) i];
        if (p.size > 0)
            midiMessages.addEvent (p.data, (int) p.size, p.sampleOffset);
    }
    hostThruScratchCount = 0;

    if (hostOutSysex.empty())
        return;

    const double now = juce::Time::getMillisecondCounterHiRes();
    int largeSysexThisBlock = 0;
    RtMidiEvent ev;
    for (;;)
    {
        const RtMidiEvent* head = hostOutSysex.peek();
        if (head == nullptr || head->dueMs > now)
            break;
        if ((int) head->size >= kLargeSysexBytes && largeSysexThisBlock >= 1)
            break;
        if (! hostOutSysex.popCopy (ev, hostCollectScratch.data(), (int) hostCollectScratch.size()))
            break;
        midiMessages.addEvent (hostCollectScratch.data(), (int) ev.size, ev.sampleOffset);
        if ((int) ev.size >= kLargeSysexBytes)
            ++largeSysexThisBlock;
    }
}

void MidiDeviceManager::handleDeviceSysex (const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return;

    const auto* d = message.getSysExData();
    const int n = message.getSysExDataSize();
    std::vector<uint8_t> full;
    full.reserve (static_cast<size_t> (n) + 2);
    full.push_back (0xf0);
    full.insert (full.end(), d, d + n);
    full.push_back (0xf7);

    if (sysexFn)
        sysexFn (full);
}

void MidiDeviceManager::handleDeviceSysexBytes (const uint8_t* data, int size, bool fragment)
{
    if (data == nullptr || size <= 0)
        return;

    // Raw bytes may be full F0..F7 or a bare SysEx body / fragment from the host.
    const bool startsF0 = data[0] == 0xf0;
    const bool endsF7 = data[size - 1] == 0xf7;

    if (! fragment && startsF0 && endsF7)
    {
        std::vector<uint8_t> full (data, data + size);
        if (sysexFn)
            sysexFn (full);
        hostSysexReassembly.clear();
        return;
    }

    if (startsF0)
        hostSysexReassembly.assign (data, data + size);
    else if (! hostSysexReassembly.empty())
        hostSysexReassembly.insert (hostSysexReassembly.end(), data, data + size);
    else
    {
        // Bare body (JUCE MidiMessage SysEx layout without F0/F7).
        hostSysexReassembly.clear();
        hostSysexReassembly.push_back (0xf0);
        hostSysexReassembly.insert (hostSysexReassembly.end(), data, data + size);
        if (! fragment)
            hostSysexReassembly.push_back (0xf7);
    }

    if (! fragment || endsF7)
    {
        if (! hostSysexReassembly.empty() && hostSysexReassembly.back() != 0xf7)
            hostSysexReassembly.push_back (0xf7);
        if (sysexFn)
            sysexFn (hostSysexReassembly);
        hostSysexReassembly.clear();
    }
}

void MidiDeviceManager::thruControllerMessageIfEnabled (const juce::MidiMessage& message,
                                                        int sampleOffset,
                                                        bool useBlockThruScratch)
{
    if (message.isSysEx())
        return;
    const auto* raw = message.getRawData();
    const int rawSize = message.getRawDataSize();
    thruControllerRawIfEnabled (raw, rawSize, sampleOffset, useBlockThruScratch);
}

void MidiDeviceManager::thruControllerRawIfEnabled (const uint8_t* raw, int rawSize, int sampleOffset,
                                                    bool useBlockThruScratch)
{
    if (raw == nullptr || rawSize < 1 || rawSize > 3)
        return;
    if (! controllerThru.load (std::memory_order_relaxed) || ! hasOutput())
        return;

    const bool noteOn = isNoteOnRaw (raw, rawSize);
    const bool noteOff = isNoteOffRaw (raw, rawSize);
    const int note = noteNumberRaw (raw, rawSize);
    if (controllerNotesToCallbacksOnly.load (std::memory_order_relaxed) && (noteOn || noteOff))
        return;

    const bool dawOut = hostOutput.load (std::memory_order_acquire);

    if (dawOut && useBlockThruScratch)
    {
        if (hostThruScratchCount < kHostThruScratchSlots)
        {
            auto& slot = hostThruScratch[(size_t) hostThruScratchCount++];
            slot.size = (uint8_t) rawSize;
            slot.sampleOffset = sampleOffset;
            std::memcpy (slot.data, raw, (size_t) rawSize);
        }
        else if (noteOff)
        {
            setThruHeldNoteBit (note, false);
            return;
        }
        else
            return;

        if (noteOn)
            setThruHeldNoteBit (note, true);
        else if (noteOff)
            setThruHeldNoteBit (note, false);
        return;
    }

    if (dawOut)
    {
        enqueueHostShort (raw, rawSize, sampleOffset);
        if (noteOn)
            setThruHeldNoteBit (note, true);
        else if (noteOff)
            setThruHeldNoteBit (note, false);
        return;
    }

    if (! thruRing.tryPush (raw, rawSize))
    {
        if (noteOff)
            setThruHeldNoteBit (note, false);
        return;
    }

    if (noteOn)
        setThruHeldNoteBit (note, true);
    else if (noteOff)
        setThruHeldNoteBit (note, false);

    requestThruFlush();
}

void MidiDeviceManager::handleControllerMessage (const juce::MidiMessage& message, bool thruAlreadyHandled)
{
    const bool isNoteOn = message.isNoteOn() && message.getVelocity() > 0;
    const bool isNoteOff = message.isNoteOff()
                        || (message.isNoteOn() && message.getVelocity() == 0);
    const int note = message.getNoteNumber();

    if (isNoteOn && noteOnFn)
        noteOnFn (note, (int) message.getVelocity());
    if (isNoteOff && noteOffFn)
        noteOffFn (note, 0);

    if (! thruAlreadyHandled)
        thruControllerMessageIfEnabled (message);

    if (isNoteOn || isNoteOff)
    {
        const auto epoch = ++thruSyncEpoch;
        juce::MessageManager::callAsync ([this, epoch, alive = alive]
        {
            if (! alive->load() || epoch != thruSyncEpoch.load())
                return; // destroyed, superseded or closed
            syncNotesSounding (notesSoundingFn ? notesSoundingFn() : false);
        });
    }
}

void MidiDeviceManager::processHostMidiInput (const juce::MidiBuffer& midiMessages)
{
    if (hostMidiSuspended.load (std::memory_order_acquire))
        return;
    if (! hostDeviceInput && ! hostControllerInput)
        return;

    const bool thruOn = controllerThru.load (std::memory_order_relaxed);

    for (const auto metadata : midiMessages)
    {
        const auto* raw = metadata.data;
        const int rawSize = metadata.numBytes;
        const int sampleOffset = metadata.samplePosition;
        if (raw == nullptr || rawSize <= 0)
            continue;

        const auto statusByte = (uint8_t) raw[0];
        const bool sysexCont = hostInSysexOpen.load (std::memory_order_relaxed) && statusByte < 0x80;
        const bool looksSysex = statusByte == 0xf0 || statusByte == 0xf7 || sysexCont;

        if (hostControllerInput && ! looksSysex && rawSize <= 3)
        {
            if (thruOn)
                thruControllerRawIfEnabled (raw, rawSize, sampleOffset, true);

            if (! hostInRing.tryPushShort (raw, rawSize, sampleOffset))
            {
                if (isNoteOffRaw (raw, rawSize))
                    setThruHeldNoteBit (noteNumberRaw (raw, rawSize), false);
                flagHostOverflow();
            }
        }

        if (hostDeviceInput && looksSysex)
        {
            const bool endsF7 = (uint8_t) raw[rawSize - 1] == 0xf7;
            const bool startsF0 = statusByte == 0xf0;
            const bool fragment = (startsF0 || sysexCont) && ! endsF7;
            if (! hostInRing.tryPushSysex (raw, rawSize, sampleOffset, 0.0, fragment))
            {
                flagHostOverflow();
                // Mid-stream drop: abort reassembly so a later F7 cannot deliver a gapped dump.
                if (fragment || sysexCont || hostInSysexOpen.load (std::memory_order_relaxed))
                {
                    hostInSysexOpen.store (false, std::memory_order_relaxed);
                    hostInSysexAbort.store (true, std::memory_order_release);
                }
            }
            else
                hostInSysexOpen.store (fragment, std::memory_order_relaxed);
        }
    }
}

void MidiDeviceManager::pumpHostMidiInput()
{
    if (hostInSysexAbort.exchange (false, std::memory_order_acq_rel))
    {
        hostSysexReassembly.clear();
        hostInSysexOpen.store (false, std::memory_order_relaxed);
    }

    RtMidiEvent ev;
    uint8_t scratch[HostMidiRing::kMaxSysexBytes];
    while (hostInRing.popCopy (ev, scratch, (int) sizeof (scratch)))
    {
        if (ev.kind == RtMidiKind::shortMsg)
        {
            juce::MidiMessage msg (ev.shortData, (int) ev.size, 0.0);
            handleControllerMessage (msg, true);
        }
        else
        {
            handleDeviceSysexBytes (scratch, (int) ev.size,
                                    ev.kind == RtMidiKind::sysexFragment);
        }
    }
}

bool MidiDeviceManager::sendMessageNowLocked (const juce::MidiMessage& message)
{
    if (hostOutput.load (std::memory_order_relaxed))
    {
        const auto* raw = message.getRawData();
        const int n = message.getRawDataSize();
        if (raw == nullptr || n <= 0)
            return false;
        if (message.isSysEx() || n > 3)
            return enqueueHostSysex (raw, n, 0.0);
        return enqueueHostShort (raw, n);
    }
    const juce::ScopedLock sl (outputLock);
    if (output == nullptr)
        return false;
    ensureBackgroundThread();
    // Immediate path for single note events - still serialized with morph egress.
    output->sendMessageNow (message);
    return true;
}

bool MidiDeviceManager::sendMessagesScheduled (const std::vector<std::vector<uint8_t>>& messages, int spacingMs)
{
    if (messages.empty())
        return true;

    if (hostOutput.load (std::memory_order_relaxed))
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        double t = juce::jmax (now, morphBusyUntilMs);
        auto wireMs = [] (const std::vector<uint8_t>& bytes)
        {
            return estimateSysexWireMs (static_cast<int> (bytes.size()));
        };
        const int gap = juce::jmax (1, spacingMs);
        for (size_t i = 0; i < messages.size(); ++i)
        {
            const auto& bytes = messages[i];
            if (! enqueueHostSysex (bytes.data(), static_cast<int> (bytes.size()), t))
                return false;
            t += static_cast<double> (wireMs (bytes));
            if (i + 1 < messages.size())
                t += static_cast<double> (gap);
        }
        morphBusyUntilMs = t;
        return true;
    }

    const juce::ScopedLock sl (outputLock);
    if (output == nullptr)
        return false;

    ensureBackgroundThread();

    const double now = juce::Time::getMillisecondCounterHiRes();
    // Never overlap a previous morph/SysEx block still draining on the wire.
    const double startMs = juce::jmax (now, morphBusyUntilMs);

    auto wireMs = [] (const std::vector<uint8_t>& bytes)
    {
        return estimateSysexWireMs (static_cast<int> (bytes.size()));
    };

    // Immediate only when the wire is actually free; otherwise schedule behind the block.
    if (messages.size() == 1 && spacingMs <= 0 && startMs <= now)
    {
        output->sendMessageNow (juce::MidiMessage (messages.front().data(),
                                                   static_cast<int> (messages.front().size())));
        morphBusyUntilMs = startMs + static_cast<double> (wireMs (messages.front()));
        return true;
    }

    juce::MidiBuffer buf;
    const int gap = juce::jmax (1, spacingMs);
    int samplePos = 0;
    for (size_t i = 0; i < messages.size(); ++i)
    {
        const auto& bytes = messages[i];
        buf.addEvent (juce::MidiMessage (bytes.data(), static_cast<int> (bytes.size())), samplePos);
        if (i + 1 < messages.size())
            samplePos += gap;
    }

    // 1000 samples/sec → MidiBuffer sample positions are milliseconds.
    output->sendBlockOfMessages (buf, startMs, 1000.0);
    // Busy until the last message has finished transmitting (start offset + wire time).
    const int durationMs = samplePos + wireMs (messages.back());
    morphBusyUntilMs = startMs + static_cast<double> (juce::jmax (1, durationMs));
    return true;
}

bool MidiDeviceManager::sendRaw (const std::vector<uint8_t>& bytes, bool pace)
{
    if (! hasOutput())
    {
        setStatus ("No MIDI output");
        return false;
    }
    std::vector<std::vector<uint8_t>> msgs { bytes };
    const int spacing = pace ? juce::jmax (1, pacingMs) : 0;
    return sendMessagesScheduled (msgs, spacing);
}

bool MidiDeviceManager::sendVoice (const VoiceData& voice, bool pace)
{
    const auto ok = sendRaw (SysexMessages::makeSingleVoiceDump (voice, channel), pace);
    if (ok)
    {
        morph.forceBaseline (voice);
        if (pace)
            setStatus ("Sent 1 voice");
    }
    return ok;
}

void MidiDeviceManager::invalidateMorphBaseline()
{
    morph.invalidateBaseline();
    if (morph.hasTarget())
        ensureHostPumpTimer();
}

bool MidiDeviceManager::sendMorphVoice (const VoiceData& voice, bool forceCommit, bool liveAllParams)
{
    if (! hasOutput())
    {
        setStatus ("No MIDI output");
        return false;
    }

    updateNotesSounding ((notesSoundingFn ? notesSoundingFn() : false) || hasThruNotesSounding());

    morph.setTarget (voice, forceCommit, liveAllParams);
    ensureHostPumpTimer();
    timerCallback();
    return true;
}

void MidiDeviceManager::timerCallback()
{
    if (hostInRing.numReady() > 0)
        pumpHostMidiInput();

    bool pumpWantsTimer = false;
    if (messagePumpFn)
        pumpWantsTimer = messagePumpFn();

    reportHostOverflowStatus();

    const bool appSounding = notesSoundingFn ? notesSoundingFn() : false;
    updateNotesSounding (appSounding || hasThruNotesSounding());

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now < morphBusyUntilMs)
        return;

    const bool needHostPorts = hostDeviceInput || hostControllerInput
                            || hostOutput.load (std::memory_order_relaxed);
    if (! morph.hasTarget() && ! morph.hasBaseline())
    {
        if (! needHostPorts && ! pumpWantsTimer && hostInRing.empty()
            && juce::MessageManager::getInstanceWithoutCreating() != nullptr)
            stopTimer();
        return;
    }

    if (morph.pendingDiffCount() > 0 && inMorphReleaseGuard())
        return;

    const auto slice = morph.tick();
    if (! slice.messages.empty())
    {
        const int spacing = slice.usedFullDump ? juce::jmax (1, pacingMs) : 1;
        sendMessagesScheduled (slice.messages, spacing);
    }

    const double after = juce::Time::getMillisecondCounterHiRes();
    if (morph.pendingDiffCount() == 0
        && (morph.hasBaseline() || ! morph.hasTarget())
        && after >= morphBusyUntilMs
        && ! needHostPorts
        && ! pumpWantsTimer
        && hostInRing.empty()
        && juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        stopTimer();
}

bool MidiDeviceManager::sendBank (const std::array<VoiceData, kBankVoiceCount>& bank)
{
    const auto ok = sendRaw (SysexMessages::makeBankDump (bank, channel));
    if (ok)
    {
        morph.invalidateBaseline();
        if (morph.hasTarget())
            ensureHostPumpTimer();
        setStatus ("Sent 32-voice bank");
    }
    return ok;
}

bool MidiDeviceManager::requestDump (bool bank32)
{
    const auto ok = sendRaw (SysexMessages::makeDumpRequest (bank32, channel));
    if (ok)
        setStatus (bank32 ? "Requested 32-voice dump..." : "Requested 1-voice dump...");
    return ok;
}

bool MidiDeviceManager::requestFunctionDump()
{
    const auto ok = sendRaw (SysexMessages::makeDumpRequestFormat (kFormatPerformance, channel));
    if (ok)
        setStatus ("Requested TX7 function dump...");
    return ok;
}

bool MidiDeviceManager::requestSystemGlobalsDump()
{
    // TX7 MIDI impl. §4-4 *1: data 0 on computeCommunication dumps Combined / CC RX /
    // Data Entry RX as param-changes plus a 1-performance bulk.
    if (! hasOutput())
    {
        setStatus ("Open MIDI out to Get RX system globals");
        return false;
    }
    if (! hasDeviceInput())
    {
        setStatus ("Open MIDI device in to receive Get RX dump");
        return false;
    }
    const auto ok = sendTxFunctionParam (TxFunctionParam::computeCommunication, 0);
    if (ok)
        setStatus ("Get RX: waiting for dump...");
    return ok;
}

bool MidiDeviceManager::sendPerformanceBulk (const Tx7PerformanceData& data, bool pace)
{
    const auto ok = sendRaw (SysexMessages::makePerformanceBulk (data, channel), pace);
    if (ok)
        setStatus ("Sent TX7 function / performance");
    return ok;
}

bool MidiDeviceManager::sendDxFunctionParam (DxFunctionParam param, uint8_t value)
{
    return sendRaw (SysexMessages::makeDxFunctionParamChange (param, value, channel), false);
}

bool MidiDeviceManager::sendTxFunctionParam (TxFunctionParam param, uint8_t value)
{
    return sendRaw (SysexMessages::makeTxFunctionParamChange (param, value, channel), false);
}

bool MidiDeviceManager::sendTxSystemState (const Tx7System::State& state)
{
    if (! hasOutput())
    {
        setStatus ("No MIDI output");
        return false;
    }
    // encodeStateForSend omits computeCommunication=0 (dump trigger). Pace like bank writes.
    const auto params = Tx7System::encodeStateForSend (state);
    std::vector<std::vector<uint8_t>> msgs;
    msgs.reserve (params.size());
    for (const auto& [param, value] : params)
        msgs.push_back (SysexMessages::makeTxFunctionParamChange (param, value, channel));
    const int spacing = juce::jmax (1, pacingMs);
    const auto ok = sendMessagesScheduled (msgs, spacing);
    if (ok)
        setStatus (state.memoryProtect ? "Sent TX7 system state (Protect ON)"
                                       : "Sent TX7 system state (Protect Off)");
    return ok;
}

bool MidiDeviceManager::sendNoteOn (int note, int velocity)
{
    if (! hasOutput())
    {
        setStatus ("No MIDI output");
        return false;
    }
    const auto n = juce::jlimit (0, 127, note);
    const auto v = juce::jlimit (1, 127, velocity);
    return sendMessageNowLocked (juce::MidiMessage::noteOn (channel, n, (juce::uint8) v));
}

bool MidiDeviceManager::sendNoteOff (int note)
{
    if (! hasOutput())
        return false;
    return sendMessageNowLocked (juce::MidiMessage::noteOff (channel, juce::jlimit (0, 127, note)));
}

void MidiDeviceManager::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (source == controllerInput.get())
    {
        handleControllerMessage (message);
        return;
    }

    if (source != input.get())
        return;

    handleDeviceSysex (message);
}

} // namespace fmlib
