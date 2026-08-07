#include "midi/MidiDeviceManager.h"
#include "sysex/SysexMessages.h"

namespace fmlib
{

MidiDeviceManager::MidiDeviceManager() = default;

MidiDeviceManager::~MidiDeviceManager()
{
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
    // Listing is queried live in get*Names
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

bool MidiDeviceManager::openOutputByName (const juce::String& name)
{
    output.reset();
    outputName.clear();
    for (auto& d : juce::MidiOutput::getAvailableDevices())
    {
        if (d.name == name)
        {
            output = juce::MidiOutput::openDevice (d.identifier);
            if (output)
            {
                outputName = name;
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
    sysexFn = nullptr;
    if (input)
        input->stop();
    input.reset();
    output.reset();
    inputName.clear();
    outputName.clear();
}

bool MidiDeviceManager::sendRaw (const std::vector<uint8_t>& bytes, bool pace)
{
    if (! output)
    {
        setStatus ("No MIDI output");
        return false;
    }
    juce::MidiMessage msg (bytes.data(), static_cast<int> (bytes.size()));
    output->sendMessageNow (msg);
    if (pace && pacingMs > 0)
        juce::Thread::sleep (pacingMs);
    return true;
}

bool MidiDeviceManager::sendVoice (const VoiceData& voice, bool pace)
{
    const auto ok = sendRaw (SysexMessages::makeSingleVoiceDump (voice, channel), pace);
    if (ok && pace)
        setStatus ("Sent 1 voice");
    return ok;
}

bool MidiDeviceManager::sendBank (const std::array<VoiceData, kBankVoiceCount>& bank)
{
    const auto ok = sendRaw (SysexMessages::makeBankDump (bank, channel));
    if (ok)
        setStatus ("Sent 32-voice bank");
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
    if (! output)
    {
        setStatus ("No MIDI output");
        return false;
    }
    const auto n = juce::jlimit (0, 127, note);
    const auto v = juce::jlimit (1, 127, velocity);
    output->sendMessageNow (juce::MidiMessage::noteOn (channel, n, (juce::uint8) v));
    return true;
}

bool MidiDeviceManager::sendNoteOff (int note)
{
    if (! output)
        return false;
    output->sendMessageNow (juce::MidiMessage::noteOff (channel, juce::jlimit (0, 127, note)));
    return true;
}

void MidiDeviceManager::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return;

    const auto* d = message.getSysExData();
    const int n = message.getSysExDataSize();
    // JUCE SysEx data excludes F0/F7 - re-wrap for parser
    std::vector<uint8_t> full;
    full.reserve (static_cast<size_t> (n) + 2);
    full.push_back (0xf0);
    full.insert (full.end(), d, d + n);
    full.push_back (0xf7);

    if (sysexFn)
        sysexFn (full);
}

} // namespace fmlib
