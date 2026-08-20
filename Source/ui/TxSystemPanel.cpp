#include "ui/TxSystemPanel.h"

namespace fmlib
{

TxSystemPanel::TxSystemPanel()
{
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centredLeft);
    hint.setFont (juce::FontOptions (13.0f));
    hint.setJustificationType (juce::Justification::topLeft);
    limitsHeader.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    switchesHeader.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    for (auto* c : std::initializer_list<juce::Component*> {
             &title, &hint, &memoryProtect, &protectOff, &limitsHeader, &noteLowLabel, &noteLow,
             &noteHighLabel, &noteHigh, &switchesHeader, &dataEntryReceive, &controlChangeReceive,
             &dataEntryVolume, &computeCommunication, &individualMode, &loadFunctionExt })
        addAndMakeVisible (*c);

    auto setupSlider = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
        s.setRange (0.0, 127.0, 1.0);
    };
    setupSlider (noteLow);
    setupSlider (noteHigh);
    noteLow.setValue (0.0, juce::dontSendNotification);
    noteHigh.setValue (127.0, juce::dontSendNotification);

    memoryProtect.setTooltip (
        "When on, TX7 blocks writing voice/function memory (needs Off before bank write).");
    protectOff.setTooltip ("Turn Memory Protect off (same helper as TX7 Globals strip).");
    noteLow.setTooltip ("Lowest MIDI note the TX7 will play (0-127). Raised above High if needed.");
    noteHigh.setTooltip ("Highest MIDI note the TX7 will play (0-127). Lowered below Low if needed.");
    dataEntryReceive.setTooltip ("Allow the TX7 to receive data-entry / incremental SysEx edits.");
    controlChangeReceive.setTooltip ("Allow the TX7 to respond to MIDI Control Change messages.");
    dataEntryVolume.setTooltip ("Route data-entry changes to volume when enabled.");
    computeCommunication.setTooltip ("Enable TX7 compute / communication switch (panel MIDI link).");
    individualMode.setTooltip (
        "Off = Combined mode (DX+TX voice pairing). On = Independent voice selection.");
    loadFunctionExt.setTooltip (
        "Which function bank loads with a voice: off = internal (INT), on = external (EXT).");

    memoryProtect.onClick = [this]
    {
        if (suppress)
            return;
        sendBool (TxFunctionParam::memoryProtect, memoryProtect.getToggleState(), 127);
    };
    protectOff.onClick = [this]
    {
        flushQueuedNoteLimits();
        setMemoryProtectUi (false);
        sendByte (TxFunctionParam::memoryProtect, 0);
        setStatus ("TX7 Memory Protect Off sent");
    };
    noteLow.onValueChange = [this]
    {
        if (suppress)
            return;
        auto low = (uint8_t) noteLow.getValue();
        const auto high = (uint8_t) noteHigh.getValue();
        if (low > high)
        {
            suppress = true;
            noteHigh.setValue ((double) low, juce::dontSendNotification);
            suppress = false;
        }
        queueNoteLimits();
    };
    noteHigh.onValueChange = [this]
    {
        if (suppress)
            return;
        auto high = (uint8_t) noteHigh.getValue();
        const auto low = (uint8_t) noteLow.getValue();
        if (high < low)
        {
            suppress = true;
            noteLow.setValue ((double) high, juce::dontSendNotification);
            suppress = false;
        }
        queueNoteLimits();
    };
    dataEntryReceive.onClick = [this]
    {
        sendBool (TxFunctionParam::dataEntryReceive, dataEntryReceive.getToggleState());
    };
    controlChangeReceive.onClick = [this]
    {
        sendBool (TxFunctionParam::controlChangeReceive, controlChangeReceive.getToggleState());
    };
    dataEntryVolume.onClick = [this]
    {
        sendBool (TxFunctionParam::dataEntryVolume, dataEntryVolume.getToggleState());
    };
    computeCommunication.onClick = [this]
    {
        sendBool (TxFunctionParam::computeCommunication, computeCommunication.getToggleState());
    };
    individualMode.onClick = [this]
    {
        sendBool (TxFunctionParam::combinedOrIndividual, individualMode.getToggleState());
    };
    loadFunctionExt.onClick = [this]
    {
        sendBool (TxFunctionParam::loadFunctionSelect, loadFunctionExt.getToggleState(), 127);
    };
}

void TxSystemPanel::setMidi (MidiDeviceManager* midiManager)
{
    midi = midiManager;
}

void TxSystemPanel::setMemoryProtectUi (bool on)
{
    suppress = true;
    memoryProtect.setToggleState (on, juce::dontSendNotification);
    suppress = false;
}

void TxSystemPanel::sendBool (TxFunctionParam param, bool on, uint8_t onValue)
{
    sendByte (param, on ? onValue : 0);
}

void TxSystemPanel::sendByte (TxFunctionParam param, uint8_t value)
{
    if (midi == nullptr)
        return;
    midi->sendTxFunctionParam (param, value);
}

void TxSystemPanel::queueNoteLimits()
{
    noteLimitsPending = true;
    startTimer (40);
}

void TxSystemPanel::flushQueuedNoteLimits()
{
    stopTimer();
    if (! noteLimitsPending)
        return;
    noteLimitsPending = false;
    const auto low = (uint8_t) noteLow.getValue();
    const auto high = (uint8_t) noteHigh.getValue();
    if (low != lastSentNoteLow)
    {
        sendByte (TxFunctionParam::noteLimitLow, low);
        lastSentNoteLow = low;
    }
    if (high != lastSentNoteHigh)
    {
        sendByte (TxFunctionParam::noteLimitHigh, high);
        lastSentNoteHigh = high;
    }
}

void TxSystemPanel::timerCallback()
{
    flushQueuedNoteLimits();
}

void TxSystemPanel::setStatus (const juce::String& s)
{
    if (onStatus)
        onStatus (s);
}

void TxSystemPanel::paint (juce::Graphics& g)
{
    g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
}

void TxSystemPanel::resized()
{
    auto r = getLocalBounds().reduced (16);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);
    hint.setBounds (r.removeFromTop (40));
    r.removeFromTop (12);

    auto protectRow = r.removeFromTop (28);
    memoryProtect.setBounds (protectRow.removeFromLeft (160));
    protectOff.setBounds (protectRow.removeFromLeft (100).reduced (2));
    r.removeFromTop (16);

    limitsHeader.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    auto lowRow = r.removeFromTop (28);
    noteLowLabel.setBounds (lowRow.removeFromLeft (48));
    noteLow.setBounds (lowRow.removeFromLeft (juce::jmin (320, lowRow.getWidth())));
    r.removeFromTop (4);
    auto highRow = r.removeFromTop (28);
    noteHighLabel.setBounds (highRow.removeFromLeft (48));
    noteHigh.setBounds (highRow.removeFromLeft (juce::jmin (320, highRow.getWidth())));
    r.removeFromTop (16);

    switchesHeader.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    const int rowH = 26;
    dataEntryReceive.setBounds (r.removeFromTop (rowH));
    controlChangeReceive.setBounds (r.removeFromTop (rowH));
    dataEntryVolume.setBounds (r.removeFromTop (rowH));
    computeCommunication.setBounds (r.removeFromTop (rowH));
    individualMode.setBounds (r.removeFromTop (rowH));
    loadFunctionExt.setBounds (r.removeFromTop (rowH));
}

} // namespace fmlib
