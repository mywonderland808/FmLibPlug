#include "midi/MidiDeviceManager.h"
#include "midi/MidiPortNames.h"
#include "sysex/Dx7Formats.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("MidiDeviceManager DAW port flags and host output queue", "[midi][host]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openInputByName (kDawMidiPortName));
    REQUIRE (midi.usesHostDeviceInput());
    REQUIRE (midi.getInputName() == kDawMidiPortName);

    REQUIRE (midi.openControllerInputByName (kDawMidiPortName));
    REQUIRE (midi.usesHostControllerInput());

    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    REQUIRE (midi.usesHostOutput());
    REQUIRE (midi.getOutputName() == kDawMidiPortName);

    REQUIRE (midi.sendNoteOn (60, 100));

    juce::MidiBuffer out;
    midi.collectHostMidiOutput (out);
    REQUIRE (out.getNumEvents() == 1);

    juce::StringArray inputs;
    inputs = midi.getInputNames (true);
    REQUIRE (inputs[0] == kDawMidiPortName);
}

TEST_CASE ("Host MIDI does not pass through unless controller thru is used", "[midi][host]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openControllerInputByName (kDawMidiPortName));
    REQUIRE (midi.openOutputByName (kDawMidiPortName));

    juce::MidiBuffer buf;
    buf.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 0);

    REQUIRE (midi.sendNoteOn (64, 90));
    buf.clear();
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 1);
}

TEST_CASE ("Forward controller MIDI to (DAW) out is same-block and skips notes in jump mode", "[midi][host]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openControllerInputByName (kDawMidiPortName));
    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    midi.setControllerThru (true);

    juce::MidiBuffer buf;
    buf.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 1);

    midi.setControllerNotesToCallbacksOnly (true);
    buf.clear();
    buf.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 0);

    buf.clear();
    buf.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 1);
}

// Host thru updates lock-free held-note bits (same helpers hardware thru uses before
// pushing ThruPods into the AbstractFifo ring — hardware send is message-thread only).
TEST_CASE ("Controller thru held-note bits track note on/off with host out", "[midi][host][thru]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openControllerInputByName (kDawMidiPortName));
    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    midi.setControllerThru (true);
    REQUIRE_FALSE (midi.hasThruNotesSounding());

    juce::MidiBuffer buf;
    buf.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 1);
    REQUIRE (midi.hasThruNotesSounding());

    buf.clear();
    buf.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    midi.exchangeHostMidi (buf);
    REQUIRE (buf.getNumEvents() == 1);
    REQUIRE_FALSE (midi.hasThruNotesSounding());
}

TEST_CASE ("(DAW) SysEx pacing holds later packets for a later block", "[midi][host]")
{
    MidiDeviceManager midi;
    REQUIRE (midi.openOutputByName (kDawMidiPortName));
    midi.setSysexPacingMs (40);

    VoiceData v {};
    REQUIRE (midi.sendVoice (v, true));
    REQUIRE (midi.sendVoice (v, true));

    juce::MidiBuffer buf;
    midi.collectHostMidiOutput (buf);
    REQUIRE (buf.getNumEvents() == 1);
}
