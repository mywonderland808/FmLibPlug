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
             &title, &hint, &getRxBtn, &resetBtn, &memoryProtect, &protectOff, &limitsHeader,
             &noteLowLabel, &noteLow, &noteHighLabel, &noteHigh, &switchesHeader, &dataEntryReceive,
             &controlChangeReceive, &dataEntryVolume, &computeCommunication, &individualMode,
             &loadFunctionExt })
        addAndMakeVisible (*c);

    auto setupSlider = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
        s.setRange (0.0, 127.0, 1.0);
    };
    setupSlider (noteLow);
    setupSlider (noteHigh);
    applyStateToUi (Tx7System::makeDefault());

    getRxBtn.setTooltip (
        "Dumps Combined / CC RX / Data Entry plus a 1-performance bulk (computeCommunication=0). "
        "Forces Compute Comm Off; Combined/pairing can change the sound. Refreshes Device TX7 Globals. "
        "Does not return note limits, Protect, or Load Function. Prefer Get Fn for performance only.");
    resetBtn.setTooltip (
        "Restore Yamaha machine defaults (Protect ON, notes C-2..G8, Combined, MIDI RX off, "
        "Load Function INT) and send them. Does not send computeCommunication=0 (that would dump). "
        "Bank writes need Protect Off afterward.");
    memoryProtect.setTooltip (
        "When on, TX7 blocks writing voice/function memory (needs Off before bank write). "
        "Not audible by itself. Yamaha power-on default is ON.");
    protectOff.setTooltip ("Turn Memory Protect off (same helper as TX7 Globals strip). Not audible by itself.");
    noteLow.setTooltip ("Lowest MIDI note the TX7 will play (0-127, factory C-2). Raised above High if needed.");
    noteHigh.setTooltip ("Highest MIDI note the TX7 will play (0-127, factory G8). Lowered below Low if needed.");
    dataEntryReceive.setTooltip (
        "Allow data-entry / incremental SysEx edits. Mutually exclusive with Data entry volume. "
        "Gates MIDI editing - not an audible voice change by itself.");
    controlChangeReceive.setTooltip (
        "Allow the TX7 to respond to MIDI Control Change messages. Gates MIDI - not audible alone.");
    dataEntryVolume.setTooltip (
        "Route data-entry changes to volume. Mutually exclusive with Data entry RX. Gates editing path.");
    computeCommunication.setTooltip (
        "On: forces Combined + CC RX + Data Entry RX (Volume off); pairing can change the sound. "
        "Off: same as Get RX (Compute Comm Off + performance dump).");
    individualMode.setTooltip (
        "Off = Combined mode (DX+TX voice pairing - can change the sound). "
        "On = Independent voice selection.");
    loadFunctionExt.setTooltip (
        "Which function bank loads with a voice: off = internal (INT), on = external (EXT). "
        "Takes effect on voice load - not audible until then.");

    getRxBtn.onClick = [this] { requestGetRx(); };
    resetBtn.onClick = [this] { resetToYamahaDefaults(); };
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
    dataEntryReceive.onClick = [this] { onDataEntryReceiveClicked(); };
    controlChangeReceive.onClick = [this]
    {
        if (suppress)
            return;
        sendBool (TxFunctionParam::controlChangeReceive, controlChangeReceive.getToggleState());
    };
    dataEntryVolume.onClick = [this] { onDataEntryVolumeClicked(); };
    computeCommunication.onClick = [this] { onComputeCommunicationClicked(); };
    individualMode.onClick = [this]
    {
        if (suppress)
            return;
        sendBool (TxFunctionParam::combinedOrIndividual, individualMode.getToggleState());
    };
    loadFunctionExt.onClick = [this]
    {
        if (suppress)
            return;
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

void TxSystemPanel::applyRemoteParam (TxFunctionParam param, uint8_t value)
{
    auto state = captureUiState();
    Tx7System::applyParam (state, param, value);
    applyStateToUi (state);
}

Tx7System::State TxSystemPanel::captureUiState() const
{
    Tx7System::State s;
    s.dataEntryReceive = dataEntryReceive.getToggleState();
    s.controlChangeReceive = controlChangeReceive.getToggleState();
    s.dataEntryVolume = dataEntryVolume.getToggleState();
    s.computeCommunication = computeCommunication.getToggleState();
    s.individualMode = individualMode.getToggleState();
    s.noteLimitLow = (uint8_t) noteLow.getValue();
    s.noteLimitHigh = (uint8_t) noteHigh.getValue();
    s.memoryProtect = memoryProtect.getToggleState();
    s.loadFunctionExt = loadFunctionExt.getToggleState();
    Tx7System::clampNoteLimits (s);
    return s;
}

void TxSystemPanel::applyStateToUi (const Tx7System::State& state)
{
    auto s = state;
    Tx7System::clampNoteLimits (s);
    suppress = true;
    dataEntryReceive.setToggleState (s.dataEntryReceive, juce::dontSendNotification);
    controlChangeReceive.setToggleState (s.controlChangeReceive, juce::dontSendNotification);
    dataEntryVolume.setToggleState (s.dataEntryVolume, juce::dontSendNotification);
    computeCommunication.setToggleState (s.computeCommunication, juce::dontSendNotification);
    individualMode.setToggleState (s.individualMode, juce::dontSendNotification);
    noteLow.setValue ((double) s.noteLimitLow, juce::dontSendNotification);
    noteHigh.setValue ((double) s.noteLimitHigh, juce::dontSendNotification);
    memoryProtect.setToggleState (s.memoryProtect, juce::dontSendNotification);
    loadFunctionExt.setToggleState (s.loadFunctionExt, juce::dontSendNotification);
    lastSentNoteLow = s.noteLimitLow;
    lastSentNoteHigh = s.noteLimitHigh;
    suppress = false;
}

void TxSystemPanel::requestGetRx()
{
    flushQueuedNoteLimits();
    if (midi == nullptr || ! midi->requestSystemGlobalsDump())
        return; // status already set by manager
    // Dump path forces computeCommunication off on the device.
    suppress = true;
    computeCommunication.setToggleState (false, juce::dontSendNotification);
    suppress = false;
}

void TxSystemPanel::resetToYamahaDefaults()
{
    flushQueuedNoteLimits();
    const auto defaults = Tx7System::makeDefault();
    applyStateToUi (defaults);
    if (midi == nullptr || ! midi->sendTxSystemState (defaults))
    {
        setStatus ("Yamaha system defaults applied locally (open MIDI out to send). Protect ON.");
        return;
    }
    setStatus ("TX7 system defaults sent (Protect ON - use Protect Off before bank write)");
}

void TxSystemPanel::onDataEntryReceiveClicked()
{
    if (suppress)
        return;
    const bool on = dataEntryReceive.getToggleState();
    if (on && dataEntryVolume.getToggleState())
    {
        suppress = true;
        dataEntryVolume.setToggleState (false, juce::dontSendNotification);
        suppress = false;
        sendByte (TxFunctionParam::dataEntryVolume, 0);
    }
    sendBool (TxFunctionParam::dataEntryReceive, on);
}

void TxSystemPanel::onDataEntryVolumeClicked()
{
    if (suppress)
        return;
    const bool on = dataEntryVolume.getToggleState();
    if (on && dataEntryReceive.getToggleState())
    {
        suppress = true;
        dataEntryReceive.setToggleState (false, juce::dontSendNotification);
        suppress = false;
        sendByte (TxFunctionParam::dataEntryReceive, 0);
    }
    sendBool (TxFunctionParam::dataEntryVolume, on);
}

void TxSystemPanel::onComputeCommunicationClicked()
{
    if (suppress)
        return;
    if (computeCommunication.getToggleState())
    {
        // §4-4: ON forces Combined + CC RX + Data Entry RX; Volume off.
        auto state = captureUiState();
        Tx7System::applyComputeCommunicationOnEffects (state);
        applyStateToUi (state);
        if (midi != nullptr)
            midi->sendTxSystemState (state); // includes computeCommunication=1
        setStatus ("Compute Comm ON (forced Combined + CC RX + Data Entry RX)");
        return;
    }
    // OFF is the dump trigger — same path as Get RX (only intentional dump sender).
    if (midi == nullptr || ! midi->requestSystemGlobalsDump())
    {
        // Toggle already flipped Off; restore if we could not start the dump.
        suppress = true;
        computeCommunication.setToggleState (true, juce::dontSendNotification);
        suppress = false;
        return;
    }
    suppress = true;
    computeCommunication.setToggleState (false, juce::dontSendNotification);
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
    hint.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto toolRow = r.removeFromTop (28);
    getRxBtn.setBounds (toolRow.removeFromLeft (72).reduced (1));
    resetBtn.setBounds (toolRow.removeFromLeft (72).reduced (1));
    toolRow.removeFromLeft (12);
    memoryProtect.setBounds (toolRow.removeFromLeft (160));
    protectOff.setBounds (toolRow.removeFromLeft (100).reduced (2));
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
