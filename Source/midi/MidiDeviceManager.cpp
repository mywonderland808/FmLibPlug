#include "midi/MidiDeviceManager.h"
#include "sysex/SysexMessages.h"
#include <cmath>

namespace fmlib
{

namespace
{
constexpr int kMorphTickMs = 20;
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

juce::StringArray MidiDeviceManager::getInputNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        names.add (d.name);
    return names;
}

juce::StringArray MidiDeviceManager::getOutputNames() const
{
    juce::StringArray names;
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        names.add (d.name);
    return names;
}

bool MidiDeviceManager::openInputByName (const juce::String& name)
{
    input.reset();
    inputName.clear();
    if (name.isEmpty())
    {
        setStatus ("MIDI device input closed");
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
    controllerInput.reset();
    controllerInputName.clear();
    clearThruHeldNotes();
    if (name.isEmpty())
    {
        setStatus ("MIDI controller input closed");
        syncNotesSounding (notesSoundingFn ? notesSoundingFn() : false);
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

bool MidiDeviceManager::openOutputByName (const juce::String& name)
{
    {
        const juce::ScopedLock sl (outputLock);
        if (output && bgThreadStarted)
            output->stopBackgroundThread();
        output.reset();
        bgThreadStarted = false;
        outputName.clear();
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
    stopTimer();
    sysexFn = nullptr;
    noteOnFn = nullptr;
    noteOffFn = nullptr;
    notesSoundingFn = nullptr;
    if (input)
        input->stop();
    if (controllerInput)
        controllerInput->stop();
    input.reset();
    controllerInput.reset();
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
    clearThruHeldNotes();
    morphBusyUntilMs = 0.0;
    notesQuietAtMs = 0.0;
    notesSoundingLatched = false;
}

void MidiDeviceManager::clearThruHeldNotes()
{
    const juce::ScopedLock sl (thruLock);
    thruHeldNotes.clear();
}

bool MidiDeviceManager::hasThruNotesSounding() const
{
    const juce::ScopedLock sl (thruLock);
    return ! thruHeldNotes.empty();
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
    ensureMorphTimer();
}

void MidiDeviceManager::setMorphByteBudget (int bytes)
{
    morph.setByteBudget (bytes);
}

void MidiDeviceManager::syncNotesSounding (bool sounding)
{
    // Thru held notes are OR'd here so MorphTransport is only written from the message thread.
    updateNotesSounding (sounding || hasThruNotesSounding());
    ensureMorphTimer();
}

void MidiDeviceManager::ensureBackgroundThread()
{
    if (output != nullptr && ! bgThreadStarted)
    {
        output->startBackgroundThread();
        bgThreadStarted = true;
    }
}

void MidiDeviceManager::ensureMorphTimer()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool needWork = morph.pendingDiffCount() > 0
                       || (morph.hasTarget() && ! morph.hasBaseline())
                       || now < morphBusyUntilMs;
    if (needWork && ! isTimerRunning())
        startTimer (kMorphTickMs);
}

bool MidiDeviceManager::sendMessageNowLocked (const juce::MidiMessage& message)
{
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
    if (output == nullptr)
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
    // Only spin the timer if there is a morph target waiting for a new baseline.
    if (morph.hasTarget())
        ensureMorphTimer();
}

bool MidiDeviceManager::sendMorphVoice (const VoiceData& voice, bool forceCommit)
{
    if (output == nullptr)
    {
        setStatus ("No MIDI output");
        return false;
    }

    updateNotesSounding ((notesSoundingFn ? notesSoundingFn() : false) || hasThruNotesSounding());

    morph.setTarget (voice, forceCommit);
    ensureMorphTimer();

    // Eager tick so idle commits feel immediate (no-op while the wire is busy).
    timerCallback();
    return true;
}

void MidiDeviceManager::timerCallback()
{
    const bool appSounding = notesSoundingFn ? notesSoundingFn() : false;
    updateNotesSounding (appSounding || hasThruNotesSounding());

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now < morphBusyUntilMs)
        return; // Prior SysEx block still on the wire; keep the timer alive.

    if (! morph.hasTarget() && ! morph.hasBaseline())
    {
        stopTimer();
        return;
    }

    // Let the released note finish decaying before the backlog lands as a dump.
    if (morph.pendingDiffCount() > 0 && inMorphReleaseGuard())
        return;

    const auto slice = morph.tick();
    if (! slice.messages.empty())
    {
        // Full dumps keep user pacing between start of dump and next traffic.
        // Param diffs use a tight gap so one tick's slice finishes before the next.
        const int spacing = slice.usedFullDump ? juce::jmax (1, pacingMs) : 1;
        sendMessagesScheduled (slice.messages, spacing);
    }

    const double after = juce::Time::getMillisecondCounterHiRes();
    if (morph.pendingDiffCount() == 0
        && (morph.hasBaseline() || ! morph.hasTarget())
        && after >= morphBusyUntilMs)
        stopTimer();
}

bool MidiDeviceManager::sendBank (const std::array<VoiceData, kBankVoiceCount>& bank)
{
    const auto ok = sendRaw (SysexMessages::makeBankDump (bank, channel));
    if (ok)
    {
        // Bank dump replaces device memory; edit-buffer baseline is unknown.
        // Invalidate without starting a morph dump until the bank finishes on the wire
        // (busy-until covers the dump; ensureMorphTimer only if a target exists).
        morph.invalidateBaseline();
        if (morph.hasTarget())
            ensureMorphTimer();
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

bool MidiDeviceManager::sendNoteOn (int note, int velocity)
{
    if (output == nullptr)
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
    if (output == nullptr)
        return false;
    return sendMessageNowLocked (juce::MidiMessage::noteOff (channel, juce::jlimit (0, 127, note)));
}

void MidiDeviceManager::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (source == controllerInput.get())
    {
        const bool isNoteOn = message.isNoteOn() && message.getVelocity() > 0;
        const bool isNoteOff = message.isNoteOff()
                            || (message.isNoteOn() && message.getVelocity() == 0);
        const int note = message.getNoteNumber();

        if (isNoteOn && noteOnFn)
            noteOnFn (note, (int) message.getVelocity());
        if (isNoteOff && noteOffFn)
            noteOffFn (note, 0);

        const bool skipNoteThru = controllerNotesToCallbacksOnly && (isNoteOn || isNoteOff);
        if (controllerThru && output != nullptr && ! message.isSysEx() && ! skipNoteThru)
        {
            sendMessageNowLocked (message);
            // Track thru notes under lock; morph sounding is updated on the message thread.
            {
                const juce::ScopedLock sl (thruLock);
                if (isNoteOn)
                    thruHeldNotes.insert (note);
                else if (isNoteOff)
                    thruHeldNotes.erase (note);
            }
            const auto epoch = ++thruSyncEpoch;
            juce::MessageManager::callAsync ([this, epoch, alive = alive]
            {
                if (! alive->load() || epoch != thruSyncEpoch.load())
                    return; // destroyed, superseded or closed
                syncNotesSounding (notesSoundingFn ? notesSoundingFn() : false);
            });
        }
        return;
    }

    if (source != input.get())
        return;

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

} // namespace fmlib
