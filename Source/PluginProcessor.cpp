#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "library/AutoTagger.h"
#include "sysex/SysexParser.h"
#include <algorithm>
#include <utility>
#include <vector>

FmLibPlugAudioProcessor::FmLibPlugAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    auditionOff.owner = this;
    auditionOn.owner = this;
    controllerNoteDelay.owner = this;
    favorites.loadFrom (prefs.favoriteIds);
    tags.loadFromFile (tagsFile());
    morphPresets.loadFromFile (morphPresetsFile());
    applyPreferencesToEngine();

    midi.setSysexReceivedCallback ([this, alive = alive] (const std::vector<uint8_t>& bytes)
    {
        juce::MessageManager::callAsync ([this, alive, bytes]
        {
            if (! alive->load())
                return; // Processor went away while the dump was queued.
            handleIncomingSysex (bytes);
        });
    });
}

FmLibPlugAudioProcessor::~FmLibPlugAudioProcessor()
{
    *alive = false;
    ++autoTagEpoch;
    autoTagCancel = true;
    if (autoTagWorker.joinable())
        autoTagWorker.join();
    auditionOn.stopTimer();
    auditionOff.stopTimer();
    releaseAllControllerNotes();
    if (lastAuditionNote >= 0)
        midi.sendNoteOff (lastAuditionNote);
    midi.setSysexReceivedCallback (nullptr);
    midi.setStatusCallback (nullptr);
    persistPreferences();
    midi.close();
}

juce::File FmLibPlugAudioProcessor::tagsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FmLibPlug")
        .getChildFile ("tags.xml");
}

juce::File FmLibPlugAudioProcessor::morphPresetsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FmLibPlug")
        .getChildFile ("morph-presets.xml");
}

const juce::String FmLibPlugAudioProcessor::getName() const
{
    return "FmLibPlug";
}

bool FmLibPlugAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void FmLibPlugAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* FmLibPlugAudioProcessor::createEditor()
{
    return new FmLibPlugAudioProcessorEditor (*this);
}

void FmLibPlugAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
    persistPreferences();
}

void FmLibPlugAudioProcessor::setStateInformation (const void*, int)
{
}

void FmLibPlugAudioProcessor::applyPreferencesToEngine()
{
    library.setBaseFolders (prefs.baseFolders);
    midi.setChannel (prefs.midiChannel);
    midi.setSysexPacingMs (prefs.sysexPacingMs);
    midi.setMorphReleaseGuardMs (prefs.morphReleaseGuardMs);
    midi.setMorphStreamMode (static_cast<fmlib::MorphStreamMode> (
        juce::jlimit (0, 1, prefs.morphStreamMode)));
    // ~one param-change per morphEmitMs slice at default budget; scale gently with emit ms.
    const int budget = juce::jlimit (14, 70, (prefs.morphEmitMs <= 50 ? 56 : (prefs.morphEmitMs >= 200 ? 28 : 42)));
    midi.setMorphByteBudget (budget);
    midi.setNotesSoundingQuery ([this] { return hasSoundingNotes(); });
    if (prefs.midiInputName.isNotEmpty())
        midi.openInputByName (prefs.midiInputName);
    if (prefs.midiControllerInputName.isNotEmpty())
        midi.openControllerInputByName (prefs.midiControllerInputName);
    if (prefs.midiOutputName.isNotEmpty())
        midi.openOutputByName (prefs.midiOutputName);
    midi.setControllerThru (prefs.midiControllerThru);
    library.rescanAsync();
}

void FmLibPlugAudioProcessor::persistPreferences()
{
    prefs.favoriteIds = favorites.toStringArray();
    prefs.midiInputName = midi.getInputName();
    prefs.midiControllerInputName = midi.getControllerInputName();
    prefs.midiOutputName = midi.getOutputName();
    prefs.midiChannel = midi.getChannel();
    prefs.sysexPacingMs = midi.getSysexPacingMs();
    prefs.baseFolders = library.getBaseFolders();
    prefs.save();
    tags.saveToFile (tagsFile());
    morphPresets.saveToFile (morphPresetsFile());
}

void FmLibPlugAudioProcessor::sendVoice (const fmlib::VoiceData& voice)
{
    const auto id = fmlib::contentIdFromVoice (voice);
    midi.sendVoice (voice);
    recent.push (id);
}

void FmLibPlugAudioProcessor::auditionNote()
{
    auditionNoteAfterDelay (0);
}

void FmLibPlugAudioProcessor::auditionNoteAfterDelay (int delayMs)
{
    playHeldNoteAfterDelay (prefs.auditionNote, prefs.auditionVelocity, delayMs);
    pendingAutoOff = true;
    pendingAutoOffMs = juce::jmax (50, prefs.auditionDurationMs);
    if (delayMs <= 0 && lastAuditionNote >= 0)
        auditionOff.startTimer (pendingAutoOffMs);
}

void FmLibPlugAudioProcessor::playHeldNoteAfterDelay (int note, int velocity, int delayMs)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    auditionOff.stopTimer();
    auditionOn.stopTimer();
    if (lastAuditionNote >= 0)
    {
        midi.sendNoteOff (lastAuditionNote);
        lastAuditionNote = -1;
    }

    pendingNote = n;
    pendingVelocity = v;
    pendingAutoOff = false;

    if (delayMs <= 0)
    {
        midi.sendNoteOn (n, v);
        lastAuditionNote = n;
        pendingNote = -1;
        midi.syncNotesSounding (hasSoundingNotes());
        return;
    }

    // Pending delayed audition is not sounding yet - leave morph idle for the lead-in dump.
    midi.syncNotesSounding (hasSoundingNotes());
    auditionOn.startTimer (juce::jmax (1, delayMs));
}

void FmLibPlugAudioProcessor::playControllerNote (int note, int velocity)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    // If this note was queued for lead-in, drop it from the pending batch.
    if (pendingControllerNote == n)
    {
        controllerNoteDelay.stopTimer();
        pendingControllerNote = -1;
        pendingControllerChord.clear();
    }
    else
    {
        pendingControllerChord.erase (
            std::remove_if (pendingControllerChord.begin(), pendingControllerChord.end(),
                            [n] (const auto& p) { return p.first == n; }),
            pendingControllerChord.end());
    }
    if (controllerSoundingNotes.count (n) > 0)
        midi.sendNoteOff (n);
    midi.sendNoteOn (n, v);
    controllerSoundingNotes.insert (n);
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::playControllerNoteAfterDelay (int note, int velocity, int delayMs)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    if (delayMs <= 0 && ! controllerNoteDelay.isTimerRunning())
    {
        playControllerNote (n, v);
        return;
    }

    // Join an in-flight lead-in so chord tones share the same delayed attack.
    if (controllerNoteDelay.isTimerRunning())
    {
        for (auto& p : pendingControllerChord)
            if (p.first == n)
            {
                p.second = v;
                return;
            }
        if (pendingControllerNote == n)
        {
            pendingControllerVelocity = v;
            return;
        }
        pendingControllerChord.push_back ({ n, v });
        return;
    }

    pendingControllerNote = n;
    pendingControllerVelocity = v;
    pendingControllerChord.clear();
    // Do not mark sounding yet - morph SysEx must land while idle.
    controllerNoteDelay.startTimer (juce::jmax (1, delayMs));
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::releaseControllerNote (int note)
{
    const int n = juce::jlimit (0, 127, note);
    if (pendingControllerNote == n)
    {
        // Promote next queued chord tone, or cancel the lead-in.
        if (! pendingControllerChord.empty())
        {
            pendingControllerNote = pendingControllerChord.front().first;
            pendingControllerVelocity = pendingControllerChord.front().second;
            pendingControllerChord.erase (pendingControllerChord.begin());
        }
        else
        {
            controllerNoteDelay.stopTimer();
            pendingControllerNote = -1;
        }
        midi.syncNotesSounding (hasSoundingNotes());
        return;
    }

    const auto it = std::find_if (pendingControllerChord.begin(), pendingControllerChord.end(),
                                  [n] (const auto& p) { return p.first == n; });
    if (it != pendingControllerChord.end())
    {
        pendingControllerChord.erase (it);
        return;
    }

    if (controllerSoundingNotes.erase (n) > 0)
        midi.sendNoteOff (n);
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::releaseAllControllerNotes()
{
    controllerNoteDelay.stopTimer();
    pendingControllerNote = -1;
    pendingControllerChord.clear();
    for (int n : controllerSoundingNotes)
        midi.sendNoteOff (n);
    controllerSoundingNotes.clear();
    midi.syncNotesSounding (hasSoundingNotes());
}

bool FmLibPlugAudioProcessor::hasPendingControllerLeadIn() const
{
    return controllerNoteDelay.isTimerRunning() || pendingControllerNote >= 0
        || ! pendingControllerChord.empty();
}

bool FmLibPlugAudioProcessor::hasSoundingNotes() const
{
    // Pending delayed note-ons are reserved but not yet on the wire - keep morph idle.
    // Thru notes are OR'd inside MidiDeviceManager::syncNotesSounding / timerCallback.
    return lastAuditionNote >= 0 || ! controllerSoundingNotes.empty();
}

void FmLibPlugAudioProcessor::ControllerNoteDelayTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr)
        return;

    auto fire = [owner = owner] (int n, int v)
    {
        if (owner->controllerSoundingNotes.count (n) > 0)
            owner->midi.sendNoteOff (n);
        owner->midi.sendNoteOn (n, v);
        owner->controllerSoundingNotes.insert (n);
    };

    if (owner->pendingControllerNote >= 0)
        fire (owner->pendingControllerNote, owner->pendingControllerVelocity);
    owner->pendingControllerNote = -1;

    for (const auto& p : owner->pendingControllerChord)
        fire (p.first, p.second);
    owner->pendingControllerChord.clear();

    owner->midi.syncNotesSounding (owner->hasSoundingNotes());
}

void FmLibPlugAudioProcessor::AuditionOnTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr || owner->pendingNote < 0)
        return;
    owner->midi.sendNoteOn (owner->pendingNote, owner->pendingVelocity);
    owner->lastAuditionNote = owner->pendingNote;
    owner->pendingNote = -1;
    owner->midi.syncNotesSounding (owner->hasSoundingNotes());
    if (owner->pendingAutoOff)
        owner->auditionOff.startTimer (owner->pendingAutoOffMs);
}

void FmLibPlugAudioProcessor::AuditionOffTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr)
        return;
    if (owner->lastAuditionNote >= 0)
    {
        owner->midi.sendNoteOff (owner->lastAuditionNote);
        owner->lastAuditionNote = -1;
        owner->midi.syncNotesSounding (owner->hasSoundingNotes());
    }
}

void FmLibPlugAudioProcessor::autoTagLibrary (std::function<void()> onDone)
{
    if (autoTagRunning.exchange (true))
        return;

    autoTagCancel = false;
    const auto epoch = ++autoTagEpoch;
    if (autoTagWorker.joinable())
        autoTagWorker.join();

    autoTagWorker = std::thread ([this, epoch, alive = alive, onDone = std::move (onDone)]
    {
        const auto entries = library.getEntriesCopy();
        std::vector<std::pair<uint64_t, std::vector<std::string>>> batch;
        batch.reserve (entries.size());
        for (const auto& e : entries)
        {
            if (autoTagCancel.load() || autoTagEpoch.load() != epoch)
                break;
            batch.emplace_back (e.contentId, fmlib::AutoTagger::tagsForVoice (e.voice, e.voiceName));
        }

        // The worker is joined in the destructor, but this batch can still be queued behind it.
        juce::MessageManager::callAsync ([this, epoch, alive, batch = std::move (batch), onDone]
        {
            if (! alive->load())
                return;
            if (autoTagEpoch.load() != epoch)
            {
                autoTagRunning = false;
                return;
            }
            // Merge only: never wipe tags the user set manually via the Tags editor.
            for (const auto& [id, tagList] : batch)
                for (const auto& t : tagList)
                    tags.addTag (id, t);
            tags.saveToFile (tagsFile());
            autoTagRunning = false;
            if (onDone)
                onDone();
        });
    });
}

void FmLibPlugAudioProcessor::handleIncomingSysex (const std::vector<uint8_t>& bytes)
{
    const auto parsed = fmlib::SysexParser::parseBytes (bytes);
    if (parsed.voices.empty())
        return;

    std::vector<fmlib::PatchEntry> entries;
    for (const auto& v : parsed.voices)
    {
        fmlib::PatchEntry e;
        e.voice = v.data;
        e.bankSlot = v.bankSlot;
        e.voiceName = fmlib::voiceNameFromData (v.data);
        e.fileName = "device";
        e.relativePath = "Device";
        e.contentId = fmlib::contentIdFromVoice (v.data);
        entries.push_back (std::move (e));
    }
    deviceBuffer.setVoices (std::move (entries));
    // Device dump replaces the edit buffer / memory view - morph diffs need a fresh baseline.
    midi.invalidateMorphBaseline();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FmLibPlugAudioProcessor();
}
