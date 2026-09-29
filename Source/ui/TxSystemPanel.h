#pragma once

#include "midi/MidiDeviceManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace fmlib
{

/**
 * TX7 machine / system parameters (g=4 param-change).
 * Get RX uses computeCommunication=0 dump (§4-4); Reset pushes Yamaha defaults
 * without sending computeCommunication=0 (dump trigger).
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
    /** Apply a g=4 param received from the TX7 (Get RX dump). */
    void applyRemoteParam (TxFunctionParam param, uint8_t value);
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
    Tx7System::State captureUiState() const;
    void applyStateToUi (const Tx7System::State& state);
    void requestGetRx();
    void resetToYamahaDefaults();
    void onDataEntryReceiveClicked();
    void onDataEntryVolumeClicked();
    void onComputeCommunicationClicked();

    MidiDeviceManager* midi = nullptr;
    bool suppress = false;
    bool noteLimitsPending = false;
    int lastSentNoteLow = -1;
    int lastSentNoteHigh = -1;

    juce::Label title { {}, "Globals" };
    juce::Label hint {
        {},
        "TX7 machine parameters (MIDI / Protect / note limits). Hover controls for details."
    };

    juce::TextButton getRxBtn { "Get RX" };
    juce::TextButton resetBtn { "Reset" };
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
