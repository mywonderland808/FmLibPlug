#include "ui/GlobalsPanel.h"

namespace fmlib
{

namespace
{
void setupSlider (juce::Slider& s, double minV, double maxV, double step)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 36, 18);
    s.setRange (minV, maxV, step);
}
} // namespace

GlobalsPanel::SheetContent::SheetContent (GlobalsPanel& o)
    : owner (o)
{
    for (auto* c : { &playHeader, &pbHeader, &ctrlHeader, &outHeader,
                     &portaTimeLabel, &pbRangeLabel, &pbStepLabel, &attenLabel })
    {
        c->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*c);
    }
    playHeader.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    pbHeader.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    ctrlHeader.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    outHeader.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    addAndMakeVisible (polyMono);
    addAndMakeVisible (portaGliss);
    addAndMakeVisible (portaMode);
    addAndMakeVisible (portaTime);
    addAndMakeVisible (pbRange);
    addAndMakeVisible (pbStep);
    addAndMakeVisible (attenuator);
    addAndMakeVisible (applyAtten);

    setupSlider (portaTime, 0.0, 99.0, 1.0);
    setupSlider (pbRange, 0.0, 12.0, 1.0);
    setupSlider (pbStep, 0.0, 12.0, 1.0);
    setupSlider (attenuator, 0.0, 7.0, 1.0);

    owner.wireControllerRow (mw, "MW", DxFunctionParam::modWheelSensitivity, DxFunctionParam::modWheelAssign,
                             Tx7Performance::kModWheelSensitivity, Tx7Performance::kModWheelAssign);
    owner.wireControllerRow (fc, "FC", DxFunctionParam::footSensitivity, DxFunctionParam::footAssign,
                             Tx7Performance::kFootSensitivity, Tx7Performance::kFootAssign);
    owner.wireControllerRow (at, "AT", DxFunctionParam::aftertouchSensitivity, DxFunctionParam::aftertouchAssign,
                             Tx7Performance::kAftertouchSensitivity, Tx7Performance::kAftertouchAssign);
    owner.wireControllerRow (bc, "BC", DxFunctionParam::breathSensitivity, DxFunctionParam::breathAssign,
                             Tx7Performance::kBreathSensitivity, Tx7Performance::kBreathAssign);

    for (auto* row : { &mw, &fc, &at, &bc })
    {
        addAndMakeVisible (row->name);
        addAndMakeVisible (row->sens);
        addAndMakeVisible (row->pitch);
        addAndMakeVisible (row->amp);
        addAndMakeVisible (row->eg);
        setupSlider (row->sens, 0.0, 15.0, 1.0);
    }

    polyMono.setTooltip ("Play mode: off = Poly, on = Mono (last-note priority).");
    portaGliss.setTooltip ("Off = smooth portamento slide. On = stepped glissando (semitones).");
    portaMode.setTooltip (
        "On = sustain-key pitch follow (poly FLW / mono full-time feel). Off = retain / fingered-style.");
    portaTime.setTooltip ("Portamento or glissando time (0 = off, 99 = slowest).");
    pbRange.setTooltip ("Pitch bend wheel range in semitones (0-12).");
    pbStep.setTooltip ("Pitch bend step size in semitones (0 = continuous glide).");
    attenuator.setTooltip (
        "Voice-A output level (0 = quietest / max atten, 7 = loudest / min atten). Needs Get Fn, then Apply (bulk).");
    applyAtten.setTooltip ("Send the Get Fn performance bulk so the attenuator value reaches the TX7.");
    mw.sens.setTooltip (
        "Modulation wheel depth (0-15). Pitch/amp need non-zero PMS/AMS in the voice data.");
    fc.sens.setTooltip (
        "Foot controller depth (0-15). Pitch/amp need non-zero PMS/AMS in the voice data.");
    at.sens.setTooltip (
        "Aftertouch depth (0-15). Pitch/amp need non-zero PMS/AMS in the voice data.");
    bc.sens.setTooltip (
        "Breath controller depth (0-15). Pitch/amp need non-zero PMS/AMS in the voice data.");

    polyMono.onClick = [this]
    {
        if (owner.suppress)
            return;
        owner.applyToggleToDevice (DxFunctionParam::polyMono,
                                   polyMono.getToggleState() ? 1 : 0,
                                   Tx7Performance::kPolyMono);
    };
    portaGliss.onClick = [this]
    {
        if (owner.suppress)
            return;
        owner.applyToggleToDevice (DxFunctionParam::portamentoGliss,
                                   portaGliss.getToggleState() ? 1 : 0,
                                   Tx7Performance::kPortamentoGliss);
    };
    portaMode.onClick = [this]
    {
        if (owner.suppress)
            return;
        owner.applyToggleToDevice (DxFunctionParam::portamentoMode,
                                   portaMode.getToggleState() ? 1 : 0,
                                   Tx7Performance::kPortamentoMode);
    };
    portaTime.onValueChange = [this]
    {
        if (owner.suppress)
            return;
        const auto v = (uint8_t) portaTime.getValue();
        owner.queueSliderParam (DxFunctionParam::portamentoTime, v, v, Tx7Performance::kPortamentoTime);
    };
    pbRange.onValueChange = [this]
    {
        if (owner.suppress)
            return;
        const auto v = (uint8_t) pbRange.getValue();
        owner.queueSliderParam (DxFunctionParam::pitchBendRange, v, v, Tx7Performance::kPitchBendRange);
    };
    pbStep.onValueChange = [this]
    {
        if (owner.suppress)
            return;
        const auto v = (uint8_t) pbStep.getValue();
        owner.queueSliderParam (DxFunctionParam::pitchBendStep, v, v, Tx7Performance::kPitchBendStep);
    };
    attenuator.onValueChange = [this]
    {
        if (owner.suppress)
            return;
        owner.writeLocalVoiceA (Tx7Performance::kAttenuator, (uint8_t) attenuator.getValue());
        owner.updateAttenApplyEnabled();
    };
    applyAtten.onClick = [this]
    {
        if (owner.onRequestApplyAttenuator)
            owner.onRequestApplyAttenuator();
    };
}

void GlobalsPanel::SheetContent::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    auto section = [&] (juce::Label& header)
    {
        header.setBounds (r.removeFromTop (20));
        r.removeFromTop (2);
    };
    section (playHeader);
    auto playRow = r.removeFromTop (26);
    polyMono.setBounds (playRow.removeFromLeft (56));
    portaGliss.setBounds (playRow.removeFromLeft (56));
    portaMode.setBounds (playRow.removeFromLeft (96));
    r.removeFromTop (2);
    auto portaRow = r.removeFromTop (26);
    portaTimeLabel.setBounds (portaRow.removeFromLeft (44));
    portaTime.setBounds (portaRow);

    r.removeFromTop (8);
    section (pbHeader);
    auto pbRow = r.removeFromTop (26);
    pbRangeLabel.setBounds (pbRow.removeFromLeft (44));
    pbRange.setBounds (pbRow.removeFromLeft (pbRow.getWidth() / 2));
    pbStepLabel.setBounds (pbRow.removeFromLeft (40));
    pbStep.setBounds (pbRow);

    r.removeFromTop (8);
    section (ctrlHeader);
    auto placeCtrl = [&] (ControllerRow& row)
    {
        auto rowR = r.removeFromTop (26);
        row.name.setBounds (rowR.removeFromLeft (28));
        row.sens.setBounds (rowR.removeFromLeft (juce::jmax (80, rowR.getWidth() - 120)));
        row.pitch.setBounds (rowR.removeFromLeft (36));
        row.amp.setBounds (rowR.removeFromLeft (36));
        row.eg.setBounds (rowR.removeFromLeft (36));
        r.removeFromTop (2);
    };
    placeCtrl (mw);
    placeCtrl (fc);
    placeCtrl (at);
    placeCtrl (bc);

    r.removeFromTop (6);
    section (outHeader);
    auto outRow = r.removeFromTop (26);
    attenLabel.setBounds (outRow.removeFromLeft (44));
    applyAtten.setBounds (outRow.removeFromRight (56).reduced (1));
    attenuator.setBounds (outRow);
}

GlobalsPanel::GlobalsPanel()
{
    title.setJustificationType (juce::Justification::centredLeft);
    syncHint.setFont (juce::FontOptions (12.0f));
    syncHint.setJustificationType (juce::Justification::centredLeft);
    syncHint.setMinimumHorizontalScale (1.0f);
    syncHint.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (title);
    addAndMakeVisible (getFn);
    addAndMakeVisible (resetFn);
    addAndMakeVisible (protectOff);
    addAndMakeVisible (applyWithLoad);
    addAndMakeVisible (syncHint);
    addAndMakeVisible (sheetViewport);
    sheetViewport.setViewedComponent (&sheet, false);
    sheetViewport.setScrollBarsShown (true, false);

    getFn.setTooltip ("Request the TX7 Voice-A function / 1-performance edit buffer (SysEx format 0x01).");
    resetFn.setTooltip (
        "Restore Yamaha factory Voice-A function defaults (poly, PB 7, MW 8+pitch, FC/AT 8, BC 15, atten 7). "
        "Live params send immediately; Atten needs Apply after Get.");
    protectOff.setTooltip ("Turn TX7 Memory Protect off so bank writes are allowed.");
    applyWithLoad.setTooltip (
        "After a voice or bank load, also send the Get Fn bulk if you edited globals since Get/Apply (default off).");
    title.setTooltip ("Drag the top edge of this panel to resize.");

    getFn.onClick = [this]
    {
        cancelPendingSlider();
        if (onRequestGet)
            onRequestGet();
    };
    resetFn.onClick = [this] { resetVoiceAToDefaults(); };
    protectOff.onClick = [this]
    {
        if (onProtectOff)
            onProtectOff();
    };
    applyWithLoad.onClick = [this]
    {
        if (onApplyWithVoiceLoadChanged)
            onApplyWithVoiceLoadChanged (applyWithLoad.getToggleState());
    };

    refreshFromBuffer();
}

void GlobalsPanel::wireControllerRow (ControllerRow& row,
                                      const juce::String& label,
                                      DxFunctionParam sensParam,
                                      DxFunctionParam assignParam,
                                      int sensIndex,
                                      int assignIndex)
{
    row.name.setText (label, juce::dontSendNotification);
    row.name.setJustificationType (juce::Justification::centredLeft);
    row.sensParam = sensParam;
    row.assignParam = assignParam;
    row.sensIndex = sensIndex;
    row.assignIndex = assignIndex;
    row.pitch.setTooltip (label + ": apply modulation to pitch (vibrato). Needs voice PMS > 0.");
    row.amp.setTooltip (label + ": apply modulation to amplitude (tremolo). Needs voice AMS > 0.");
    row.eg.setTooltip (label + ": apply modulation to EG bias (tone).");

    row.sens.onValueChange = [this, &row]
    {
        if (suppress)
            return;
        const auto stored = (uint8_t) row.sens.getValue();
        queueSliderParam (row.sensParam,
                          stored,
                          Tx7Performance::controllerSensitivityToLive (stored),
                          row.sensIndex);
    };
    auto assignClick = [this, &row]
    {
        if (suppress)
            return;
        applyControllerAssign (row);
    };
    row.pitch.onClick = assignClick;
    row.amp.onClick = assignClick;
    row.eg.onClick = assignClick;
}

void GlobalsPanel::setFunctionBuffer (FunctionBuffer* buffer)
{
    buf = buffer;
    refreshFromBuffer();
}

void GlobalsPanel::setMidi (MidiDeviceManager* midiManager)
{
    midi = midiManager;
}

void GlobalsPanel::setApplyWithVoiceLoad (bool on)
{
    applyWithLoad.setToggleState (on, juce::dontSendNotification);
}

void GlobalsPanel::refreshFromBuffer()
{
    cancelPendingSlider();
    const bool wasHint = syncHint.isVisible();
    syncControlsFromBuffer();
    const bool synced = buf != nullptr && buf->canBulkSend();
    updateAttenApplyEnabled();
    syncHint.setVisible (! synced);
    applyWithLoad.setTooltip (
        synced ? "After a voice or bank load, also send the Get Fn bulk if you edited globals since Get/Apply."
               : "Get Fn from the TX7 first - apply-with-load will not send until then.");
    if (wasHint != syncHint.isVisible())
        resized();
    repaint();
}

void GlobalsPanel::syncControlsFromBuffer()
{
    suppress = true;
    static const Tx7PerformanceData kFallback = Tx7Performance::makeDefault();
    const auto& d = buf != nullptr ? buf->getData() : kFallback;
    sheet.polyMono.setToggleState (Tx7Performance::polyMono (d) != 0, juce::dontSendNotification);
    sheet.pbRange.setValue ((double) Tx7Performance::pitchBendRange (d), juce::dontSendNotification);
    sheet.pbStep.setValue ((double) Tx7Performance::pitchBendStep (d), juce::dontSendNotification);
    sheet.portaGliss.setToggleState (Tx7Performance::portamentoGliss (d) != 0, juce::dontSendNotification);
    sheet.portaMode.setToggleState (Tx7Performance::portamentoMode (d) != 0, juce::dontSendNotification);
    sheet.portaTime.setValue ((double) Tx7Performance::portamentoTime (d), juce::dontSendNotification);
    sheet.attenuator.setValue ((double) Tx7Performance::attenuator (d), juce::dontSendNotification);

    auto syncCtrl = [&] (ControllerRow& row, uint8_t sens, uint8_t assign)
    {
        row.sens.setValue ((double) sens, juce::dontSendNotification);
        row.pitch.setToggleState ((assign & Tx7Performance::kAssignPitch) != 0, juce::dontSendNotification);
        row.amp.setToggleState ((assign & Tx7Performance::kAssignAmp) != 0, juce::dontSendNotification);
        row.eg.setToggleState ((assign & Tx7Performance::kAssignEgBias) != 0, juce::dontSendNotification);
    };
    syncCtrl (sheet.mw, Tx7Performance::modWheelSensitivity (d), Tx7Performance::modWheelAssign (d));
    syncCtrl (sheet.fc, Tx7Performance::footSensitivity (d), Tx7Performance::footAssign (d));
    syncCtrl (sheet.at, Tx7Performance::aftertouchSensitivity (d), Tx7Performance::aftertouchAssign (d));
    syncCtrl (sheet.bc, Tx7Performance::breathSensitivity (d), Tx7Performance::breathAssign (d));
    suppress = false;
}

void GlobalsPanel::writeLocalVoiceA (int bulkIndex, uint8_t value)
{
    if (buf == nullptr)
        return;
    buf->ensureLocalScratch();
    Tx7Performance::setVoiceAField (buf->getData(), bulkIndex, value);
    if (buf->hasDeviceSnapshot())
        buf->markDirty();
    updateAttenApplyEnabled();
}

void GlobalsPanel::updateAttenApplyEnabled()
{
    sheet.applyAtten.setEnabled (buf != nullptr && buf->canBulkSend());
}

void GlobalsPanel::pushAllLiveParams()
{
    if (midi == nullptr || buf == nullptr || ! buf->known())
        return;
    const auto& d = buf->getData();
    midi->sendDxFunctionParam (DxFunctionParam::polyMono, Tx7Performance::polyMono (d));
    midi->sendDxFunctionParam (DxFunctionParam::pitchBendRange, Tx7Performance::pitchBendRange (d));
    midi->sendDxFunctionParam (DxFunctionParam::pitchBendStep, Tx7Performance::pitchBendStep (d));
    midi->sendDxFunctionParam (DxFunctionParam::portamentoMode, Tx7Performance::portamentoMode (d));
    midi->sendDxFunctionParam (DxFunctionParam::portamentoGliss, Tx7Performance::portamentoGliss (d));
    midi->sendDxFunctionParam (DxFunctionParam::portamentoTime, Tx7Performance::portamentoTime (d));
    midi->sendDxFunctionParam (DxFunctionParam::modWheelSensitivity,
                               Tx7Performance::controllerSensitivityToLive (Tx7Performance::modWheelSensitivity (d)));
    midi->sendDxFunctionParam (DxFunctionParam::modWheelAssign, Tx7Performance::modWheelAssign (d));
    midi->sendDxFunctionParam (DxFunctionParam::footSensitivity,
                               Tx7Performance::controllerSensitivityToLive (Tx7Performance::footSensitivity (d)));
    midi->sendDxFunctionParam (DxFunctionParam::footAssign, Tx7Performance::footAssign (d));
    midi->sendDxFunctionParam (DxFunctionParam::aftertouchSensitivity,
                               Tx7Performance::controllerSensitivityToLive (Tx7Performance::aftertouchSensitivity (d)));
    midi->sendDxFunctionParam (DxFunctionParam::aftertouchAssign, Tx7Performance::aftertouchAssign (d));
    midi->sendDxFunctionParam (DxFunctionParam::breathSensitivity,
                               Tx7Performance::controllerSensitivityToLive (Tx7Performance::breathSensitivity (d)));
    midi->sendDxFunctionParam (DxFunctionParam::breathAssign, Tx7Performance::breathAssign (d));
}

void GlobalsPanel::resetVoiceAToDefaults()
{
    flushQueuedSlider();
    if (buf == nullptr)
        return;
    buf->ensureLocalScratch();
    Tx7Performance::applyVoiceADefaults (buf->getData());
    if (buf->hasDeviceSnapshot())
        buf->markDirty();
    syncControlsFromBuffer();
    updateAttenApplyEnabled();
    pushAllLiveParams();
    setStatus (buf->canBulkSend()
                   ? "Voice-A defaults sent live - Apply next to Atten for bulk attenuator"
                   : "Voice-A defaults sent live - Get Fn first if you need Atten Apply");
}

void GlobalsPanel::applyToggleToDevice (DxFunctionParam param, uint8_t value, int bulkIndex)
{
    flushQueuedSlider();
    writeLocalVoiceA (bulkIndex, value);
    if (midi != nullptr)
        midi->sendDxFunctionParam (param, value);
}

void GlobalsPanel::applyControllerAssign (ControllerRow& row)
{
    uint8_t assign = 0;
    if (row.pitch.getToggleState())
        assign = static_cast<uint8_t> (assign | Tx7Performance::kAssignPitch);
    if (row.amp.getToggleState())
        assign = static_cast<uint8_t> (assign | Tx7Performance::kAssignAmp);
    if (row.eg.getToggleState())
        assign = static_cast<uint8_t> (assign | Tx7Performance::kAssignEgBias);
    applyToggleToDevice (row.assignParam, assign, row.assignIndex);
}

void GlobalsPanel::queueSliderParam (DxFunctionParam param, uint8_t bulkValue, uint8_t midiValue, int bulkIndex)
{
    if (pendingSlider.has_value() && pendingSlider->param != param)
        flushQueuedSlider();

    writeLocalVoiceA (bulkIndex, bulkValue);
    pendingSlider = PendingSlider { param, midiValue };
    startTimer (40);
}

void GlobalsPanel::cancelPendingSlider()
{
    stopTimer();
    pendingSlider.reset();
}

void GlobalsPanel::flushQueuedSlider()
{
    stopTimer();
    if (! pendingSlider.has_value())
        return;
    const auto p = *pendingSlider;
    pendingSlider.reset();
    if (midi != nullptr)
        midi->sendDxFunctionParam (p.param, p.midiValue);
}

void GlobalsPanel::timerCallback()
{
    flushQueuedSlider();
}

void GlobalsPanel::setStatus (const juce::String& s)
{
    if (onStatus)
        onStatus (s);
}

bool GlobalsPanel::isInResizeGrip (juce::Point<int> localPos) const
{
    return localPos.y >= 0 && localPos.y < kResizeGripH;
}

void GlobalsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! isInResizeGrip (e.getPosition()))
        return;
    resizing = true;
    resizeStartHeight = getHeight();
    resizeStartY = e.getScreenY();
}

void GlobalsPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! resizing || onHeightChanged == nullptr)
        return;
    // Dragging the top edge down shrinks the panel; up grows it.
    const int delta = resizeStartY - e.getScreenY();
    onHeightChanged (resizeStartHeight + delta);
}

void GlobalsPanel::mouseUp (const juce::MouseEvent&)
{
    if (resizing && onHeightChangeEnded)
        onHeightChangeEnded();
    resizing = false;
}

void GlobalsPanel::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInResizeGrip (e.getPosition()) ? juce::MouseCursor::UpDownResizeCursor
                                                     : juce::MouseCursor::NormalCursor);
}

void GlobalsPanel::mouseExit (const juce::MouseEvent&)
{
    if (! resizing)
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void GlobalsPanel::paint (juce::Graphics& g)
{
    auto c = findColour (juce::ResizableWindow::backgroundColourId).brighter (0.04f);
    g.setColour (c);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    // Top resize grip.
    auto grip = getLocalBounds().removeFromTop (kResizeGripH);
    g.setColour (findColour (juce::Label::textColourId).withAlpha (0.35f));
    const int midY = grip.getCentreY();
    const int cx = grip.getCentreX();
    g.fillRect (cx - 12, midY - 1, 24, 1);
    g.fillRect (cx - 8, midY + 1, 16, 1);
}

void GlobalsPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kResizeGripH);
    r = r.reduced (8);
    title.setBounds (r.removeFromTop (18));
    r.removeFromTop (2);
    auto row1 = r.removeFromTop (26);
    getFn.setBounds (row1.removeFromLeft (60).reduced (1));
    resetFn.setBounds (row1.removeFromLeft (72).reduced (1));
    protectOff.setBounds (row1.removeFromLeft (84).reduced (1));
    r.removeFromTop (2);
    applyWithLoad.setBounds (r.removeFromTop (22));
    if (syncHint.isVisible())
    {
        r.removeFromTop (2);
        syncHint.setBounds (r.removeFromTop (18));
    }
    else
    {
        syncHint.setBounds ({});
    }

    r.removeFromTop (4);
    sheetViewport.setBounds (r);
    sheet.setSize (juce::jmax (200, sheetViewport.getWidth() - sheetViewport.getScrollBarThickness()),
                   sheet.contentHeight());
}

} // namespace fmlib
