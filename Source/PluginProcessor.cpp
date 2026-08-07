#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "library/AutoTagger.h"
#include "sysex/SysexParser.h"
#include <utility>
#include <vector>

FmLibPlugAudioProcessor::FmLibPlugAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    auditionOff.owner = this;
    favorites.loadFrom (prefs.favoriteIds);
    tags.loadFromFile (tagsFile());
    morphPresets.loadFromFile (morphPresetsFile());
    applyPreferencesToEngine();

    midi.setSysexReceivedCallback ([this] (const std::vector<uint8_t>& bytes)
    {
        juce::MessageManager::callAsync ([this, bytes]
        {
            handleIncomingSysex (bytes);
        });
    });
}

FmLibPlugAudioProcessor::~FmLibPlugAudioProcessor()
{
    ++autoTagEpoch;
    autoTagCancel = true;
    if (autoTagWorker.joinable())
        autoTagWorker.join();
    auditionOff.stopTimer();
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
    if (prefs.midiInputName.isNotEmpty())
        midi.openInputByName (prefs.midiInputName);
    if (prefs.midiOutputName.isNotEmpty())
        midi.openOutputByName (prefs.midiOutputName);
    library.rescanAsync();
}

void FmLibPlugAudioProcessor::persistPreferences()
{
    prefs.favoriteIds = favorites.toStringArray();
    prefs.midiInputName = midi.getInputName();
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
    const int note = prefs.auditionNote;
    const int vel = prefs.auditionVelocity;
    const int ms = juce::jmax (50, prefs.auditionDurationMs);
    auditionOff.stopTimer();
    if (lastAuditionNote >= 0)
        midi.sendNoteOff (lastAuditionNote);
    midi.sendNoteOn (note, vel);
    lastAuditionNote = note;
    auditionOff.startTimer (ms);
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

    autoTagWorker = std::thread ([this, epoch, onDone = std::move (onDone)]
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

        juce::MessageManager::callAsync ([this, epoch, batch = std::move (batch), onDone]
        {
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
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FmLibPlugAudioProcessor();
}
