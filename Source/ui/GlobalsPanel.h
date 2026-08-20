#pragma once

#include "library/FunctionBuffer.h"
#include "midi/MidiDeviceManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace fmlib
{

/**
 * TX7 Voice-A performance globals under the device buffer:
 * toolbar + scrollable Play / PB / Controllers / Output. Height is parent-driven;
 * drag the top grip to resize.
 */
class GlobalsPanel : public juce::Component,
                     private juce::Timer
{
public:
    using StatusFn = std::function<void(const juce::String&)>;
    using BoolFn = std::function<void(bool)>;
    using HeightFn = std::function<void(int)>;

    static constexpr int kMinHeight = 140;
    static constexpr int kDefaultHeight = 280;
    static constexpr int kMaxHeight = 560;
    static constexpr int kResizeGripH = 6;

    GlobalsPanel();

    void setFunctionBuffer (FunctionBuffer* buffer);
    void setMidi (MidiDeviceManager* midiManager);
    void refreshFromBuffer();
    void setApplyWithVoiceLoad (bool on);
    bool getApplyWithVoiceLoad() const { return applyWithLoad.getToggleState(); }

    StatusFn onStatus;
    BoolFn onApplyWithVoiceLoadChanged;
    /** Parent should clamp, persist, and re-layout with the new panel height. */
    HeightFn onHeightChanged;
    std::function<void()> onHeightChangeEnded;
    std::function<void()> onRequestGet;
    /** Send performance bulk so Voice-A attenuator (and buffer) reach the TX7. */
    std::function<void()> onRequestApplyAttenuator;
    std::function<void()> onProtectOff;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    struct ControllerRow
    {
        juce::Label name;
        juce::Slider sens;
        juce::ToggleButton pitch { "P" }, amp { "A" }, eg { "E" };
        DxFunctionParam sensParam {};
        DxFunctionParam assignParam {};
        int sensIndex = 0;
        int assignIndex = 0;
    };

    class SheetContent : public juce::Component
    {
    public:
        explicit SheetContent (GlobalsPanel& owner);
        void resized() override;
        int contentHeight() const
        {
            // Keep in sync with SheetContent::resized() vertical budget (+ padding).
            constexpr int padY = 4; // reduced (4, 2) top+bottom
            constexpr int header = 22; // 20 + 2
            constexpr int row = 26;
            constexpr int ctrlGap = 2;
            constexpr int sectionGap = 8;
            constexpr int outGap = 6;
            const int play = header + row + 2 + row;
            const int pb = header + row;
            const int ctrl = header + 4 * (row + ctrlGap);
            const int out = header + row;
            return padY + play + sectionGap + pb + sectionGap + ctrl + outGap + out;
        }

        juce::Label playHeader { {}, "Play" };
        juce::ToggleButton polyMono { "Mono" };
        juce::ToggleButton portaGliss { "Gliss" };
        juce::ToggleButton portaMode { "Porta follow" };
        juce::Label portaTimeLabel { {}, "Porta" };
        juce::Slider portaTime;

        juce::Label pbHeader { {}, "Pitch bend" };
        juce::Label pbRangeLabel { {}, "Range" };
        juce::Slider pbRange;
        juce::Label pbStepLabel { {}, "Step" };
        juce::Slider pbStep;

        juce::Label ctrlHeader { {}, "Controllers" };
        ControllerRow mw, fc, at, bc;

        juce::Label outHeader { {}, "Output" };
        juce::Label attenLabel { {}, "Atten" };
        juce::Slider attenuator;
        juce::TextButton applyAtten { "Apply" };

    private:
        GlobalsPanel& owner;
    };

    void timerCallback() override;
    void syncControlsFromBuffer();
    void cancelPendingSlider();
    void writeLocalVoiceA (int bulkIndex, uint8_t value);
    void applyToggleToDevice (DxFunctionParam param, uint8_t value, int bulkIndex);
    void applyControllerAssign (ControllerRow& row);
    void queueSliderParam (DxFunctionParam param, uint8_t bulkValue, uint8_t midiValue, int bulkIndex);
    void flushQueuedSlider();
    void resetVoiceAToDefaults();
    void pushAllLiveParams();
    void updateAttenApplyEnabled();
    void setStatus (const juce::String& s);
    void wireControllerRow (ControllerRow& row,
                             const juce::String& label,
                             DxFunctionParam sensParam,
                             DxFunctionParam assignParam,
                             int sensIndex,
                             int assignIndex);
    bool isInResizeGrip (juce::Point<int> localPos) const;

    FunctionBuffer* buf = nullptr;
    MidiDeviceManager* midi = nullptr;
    bool suppress = false;
    bool resizing = false;
    int resizeStartHeight = 0;
    int resizeStartY = 0;

    struct PendingSlider
    {
        DxFunctionParam param {};
        uint8_t midiValue = 0;
    };
    std::optional<PendingSlider> pendingSlider;

    juce::Label title { {}, "TX7 Globals" };
    juce::TextButton getFn { "Get Fn" }, resetFn { "Reset Fn" }, protectOff { "Protect Off" };
    juce::ToggleButton applyWithLoad { "Apply with voice load" };
    juce::Label syncHint {
        {},
        "Get Fn before Apply (attenuator) or apply-with-load."
    };

    juce::Viewport sheetViewport;
    SheetContent sheet { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalsPanel)
};

} // namespace fmlib
