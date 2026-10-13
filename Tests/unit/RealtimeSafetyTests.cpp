#define FMLIBPLUG_RT_ALLOC_GUARD 1
#include "AllocGuard.h"

#include "midi/MidiDeviceManager.h"
#include "midi/MidiPortNames.h"
#include "sysex/Dx7Formats.h"
#include "sysex/SysexMessages.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace fmlib;
using namespace fmlib::test;

TEST_CASE ("exchangeHostMidi allocates nothing for 1000 host CCs", "[midi][rt][alloc]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openControllerInputByName (kDawMidiPortName));
    REQUIRE (midi.openOutputByName (kDawMidiPortName));

    juce::MidiBuffer buf;
    for (int i = 0; i < 1000; ++i)
        buf.addEvent (juce::MidiMessage::controllerEvent (1, 1, i & 0x7f), i);

    {
        ScopedRtGuard guard;
        midi.exchangeHostMidi (buf);
        REQUIRE (guard.allocations() == 0);
    }

    midi.pumpHostMidiInput();
}

TEST_CASE ("Host note overtakes paced SysEx still waiting", "[midi][rt][order]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    midi.setSysexPacingMs (40);

    VoiceData v {};
    REQUIRE (midi.sendVoice (v, true));
    REQUIRE (midi.sendVoice (v, true));

    REQUIRE (midi.sendNoteOn (60, 100));

    juce::MidiBuffer buf;
    midi.collectHostMidiOutput (buf);

    bool sawNote = false;
    int sysexCount = 0;
    for (const auto meta : buf)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            sawNote = true;
        if (msg.isSysEx())
            ++sysexCount;
    }

    REQUIRE (sawNote);
    // First paced voice is due; second waits. Note may share the block with the first SysEx.
    REQUIRE (sysexCount <= 1);
}

TEST_CASE ("Host SysEx bulk in then pump delivers to callback", "[midi][rt]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openInputByName (kDawMidiPortName));

    std::vector<uint8_t> received;
    midi.setSysexReceivedCallback ([&] (const std::vector<uint8_t>& bytes)
    {
        received = bytes;
    });

    const auto dump = SysexMessages::makeSingleVoiceDump (VoiceData {}, 1);
    juce::MidiBuffer buf;
    buf.addEvent (juce::MidiMessage (dump.data(), (int) dump.size()), 0);

    {
        ScopedRtGuard guard;
        midi.exchangeHostMidi (buf);
        REQUIRE (guard.allocations() == 0);
    }

    midi.pumpHostMidiInput();
    REQUIRE (received.size() == dump.size());
    REQUIRE (received.front() == 0xf0);
    REQUIRE (received.back() == 0xf7);
}

TEST_CASE ("Host SysEx fragments reassemble via pumpHostMidiInput", "[midi][rt]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openInputByName (kDawMidiPortName));

    std::vector<uint8_t> received;
    midi.setSysexReceivedCallback ([&] (const std::vector<uint8_t>& bytes)
    {
        received = bytes;
    });

    const auto dump = SysexMessages::makeSingleVoiceDump (VoiceData {}, 1);
    REQUIRE (dump.size() > 40);

    REQUIRE (midi.testPushDeviceSysex (dump.data(), 40, true));
    midi.pumpHostMidiInput();
    REQUIRE (received.empty());

    REQUIRE (midi.testPushDeviceSysex (dump.data() + 40, (int) dump.size() - 40, false));
    midi.pumpHostMidiInput();
    REQUIRE (received.size() == dump.size());
    REQUIRE (received.front() == 0xf0);
    REQUIRE (received.back() == 0xf7);
}

TEST_CASE ("Multiple due small SysEx drain in one collect; large capped at one", "[midi][rt]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    midi.setSysexPacingMs (0);

    // Three tiny messages; after wire-time elapses they are all due together.
    for (int i = 0; i < 3; ++i)
    {
        std::vector<uint8_t> tiny { 0xf0, 0x43, 0x10, 0x00, (uint8_t) i, 0xf7 };
        REQUIRE (midi.sendRaw (tiny, false));
    }
    juce::Thread::sleep (40);

    juce::MidiBuffer buf;
    midi.collectHostMidiOutput (buf);
    REQUIRE (buf.getNumEvents() == 3);

    buf.clear();
    std::array<VoiceData, kBankVoiceCount> bank {};
    REQUIRE (midi.sendBank (bank));
    REQUIRE (midi.sendBank (bank));
    midi.collectHostMidiOutput (buf);
    int sysex = 0;
    for (const auto meta : buf)
        if (meta.getMessage().isSysEx())
            ++sysex;
    REQUIRE (sysex == 1);
}

TEST_CASE ("collectHostMidiOutput with pending bulk stays allocation-free", "[midi][rt][alloc]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openOutputByName (kDawMidiPortName));

    std::array<VoiceData, kBankVoiceCount> bank {};
    REQUIRE (midi.sendBank (bank));

    juce::MidiBuffer buf;
    // Host buffers are expected to have capacity; warm it outside the RT guard.
    buf.ensureSize (HostMidiRing::kMaxSysexBytes + 4096);
    {
        ScopedRtGuard guard;
        midi.collectHostMidiOutput (buf);
        REQUIRE (guard.allocations() == 0);
    }
    REQUIRE (buf.getNumEvents() >= 1);
}

TEST_CASE ("mid-fragment host overflow aborts reassembly", "[midi][rt]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openInputByName (kDawMidiPortName));

    int deliveries = 0;
    std::vector<uint8_t> last;
    midi.setSysexReceivedCallback ([&] (const std::vector<uint8_t>& bytes)
    {
        ++deliveries;
        last = bytes;
    });

    const auto dump = SysexMessages::makeSingleVoiceDump (VoiceData {}, 1);
    REQUIRE (dump.size() > 80);

    REQUIRE (midi.testPushDeviceSysex (dump.data(), 40, true));

    // Fill the in-ring until a fragment push fails (overflow).
    std::vector<uint8_t> pad { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    int pushed = 0;
    while (midi.testPushDeviceSysex (pad.data(), (int) pad.size(), true))
        ++pushed;
    REQUIRE (pushed > 0);

    // Continuation ending in F7 must not complete a gapped dump after abort.
    std::vector<uint8_t> tail (dump.begin() + 40, dump.end());
    juce::MidiBuffer cont;
    cont.addEvent (juce::MidiMessage (tail.data(), (int) tail.size()), 0);
    midi.exchangeHostMidi (cont);

    midi.pumpHostMidiInput();
    REQUIRE (deliveries == 0); // no gapped / corrupt dump after mid-fragment overflow

    // Fresh complete dump still works.
    juce::MidiBuffer full;
    full.addEvent (juce::MidiMessage (dump.data(), (int) dump.size()), 0);
    midi.exchangeHostMidi (full);
    midi.pumpHostMidiInput();
    REQUIRE (deliveries == 1);
    REQUIRE (last.size() == dump.size());
    REQUIRE (last.front() == 0xf0);
    REQUIRE (last.back() == 0xf7);
}
