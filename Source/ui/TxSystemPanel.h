#pragma once

#include "midi/MidiDeviceManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace fmlib
{

/**
 * TX7 machine / system parameters (g=4 param-change). Live-only; not part of FunctionBuffer.
 */
class TxSystemPanel : public juce::Component,
                      private juce::Timer
{
public:
    using StatusFn = std::function<void(const juce::String&)>;

    TxSystemPanel();

    void setMidi (MidiDeviceManager* midiManager);
    /** Keep Protect toggle in sync when Protect Off is sent from Globals strip. */
    void setMemoryProtectUi (bool on);
    StatusFn onStatus;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void sendBool (TxFunctionParam param, bool on, uint8_t onValue = 1);
    void sendByte (TxFunctionParam param, uint8_t value);
    void queueNoteLimits();
    void flushQueuedNoteLimits();
    void setStatus (const juce::String& s);

    MidiDeviceManager* midi = nullptr;
    bool suppress = false;
    bool noteLimitsPending = false;
    int lastSentNoteLow = -1;
    int lastSentNoteHigh = -1;

    juce::Label title { {}, "Globals" };
    juce::Label hint {
        {},
        "TX7 machine parameters (live g=4). Not part of Get Fn / Attenuator Apply."
    };

    juce::ToggleButton memoryProtect { "Memory Protect" };
    juce::TextButton protectOff { "Protect Off" };

    juce::Label limitsHeader { {}, "Note limits" };
    juce::Label noteLowLabel { {}, "Low" };
    juce::Slider noteLow;
    juce::Label noteHighLabel { {}, "High" };
    juce::Slider noteHigh;

    juce::Label switchesHeader { {}, "MIDI / mode" };
    juce::ToggleButton dataEntryReceive { "Data entry RX" };
    juce::ToggleButton controlChangeReceive { "CC RX" };
    juce::ToggleButton dataEntryVolume { "Data entry volume" };
    juce::ToggleButton computeCommunication { "Compute comm" };
    juce::ToggleButton individualMode { "Individual (vs Combined)" };
    juce::ToggleButton loadFunctionExt { "Load function EXT" };
};

} // namespace fmlib
