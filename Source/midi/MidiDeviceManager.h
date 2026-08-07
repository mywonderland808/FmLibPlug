#pragma once

#include "sysex/Dx7Formats.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

namespace fmlib
{

class MidiDeviceManager : private juce::MidiInputCallback
{
public:
    using StatusFn = std::function<void(const juce::String&)>;
    using SysexFn = std::function<void(const std::vector<uint8_t>&)>;

    MidiDeviceManager();
    ~MidiDeviceManager() override;

    void refreshDevices();
    juce::StringArray getInputNames() const;
    juce::StringArray getOutputNames() const;

    bool openInputByName (const juce::String& name);
    bool openOutputByName (const juce::String& name);
    void close();

    void setChannel (int channel1to16) { channel = juce::jlimit (1, 16, channel1to16); }
    int getChannel() const { return channel; }

    void setSysexPacingMs (int ms) { pacingMs = juce::jmax (0, ms); }
    int getSysexPacingMs() const { return pacingMs; }

    bool sendRaw (const std::vector<uint8_t>& bytes);
    bool sendVoice (const VoiceData& voice);
    bool sendBank (const std::array<VoiceData, kBankVoiceCount>& bank);
    bool requestDump (bool bank32);
    bool sendNoteOn (int note, int velocity);
    bool sendNoteOff (int note);

    void setStatusCallback (StatusFn fn) { statusFn = std::move (fn); }
    void setSysexReceivedCallback (SysexFn fn) { sysexFn = std::move (fn); }

    juce::String getInputName() const { return inputName; }
    juce::String getOutputName() const { return outputName; }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;
    void setStatus (const juce::String& s);

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String inputName, outputName;
    int channel = 1;
    int pacingMs = 20;
    juce::String status { "No MIDI devices selected" };
    StatusFn statusFn;
    SysexFn sysexFn;
};

} // namespace fmlib
