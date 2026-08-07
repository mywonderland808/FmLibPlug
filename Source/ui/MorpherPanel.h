#pragma once

#include "library/MorphPresetStore.h"
#include "sysex/Dx7Formats.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace fmlib
{

class MorpherPanel : public juce::Component,
                     private juce::ListBoxModel
{
public:
    using MorphFn = std::function<void(const VoiceData&)>;
    using PersistFn = std::function<void()>;

    MorpherPanel();

    enum class Mode { morph, presets };

    void setMode (Mode m);
    Mode getMode() const { return mode; }

    void setCorner (int corner0to3, const VoiceData& v, const juce::String& name);
    void clearCorners();
    void setMarkerPosition (float x, float y);
    void setPresetStore (MorphPresetStore* store);
    MorphPreset currentAsPreset (const juce::String& name) const;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    MorphFn onMorph;
    PersistFn onPresetsChanged;
    /** Called when user wants to assign library selection to a corner (0..3). */
    std::function<void(int)> onRequestAssignCorner;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void updateFromPoint (juce::Point<float> p, bool forceEmit);
    void emitMorph();
    void refreshPresetList();
    void saveCurrentPreset();
    void loadSelectedPreset();
    void deleteSelectedPreset();
    juce::String cornerDisplayName (int corner) const;

    Mode mode = Mode::morph;
    VoiceData corners[4] {};
    juce::String cornerNames[4];
    bool cornerSet[4] { false, false, false, false };
    float posX = 0.0f, posY = 0.0f;
    uint32_t lastMorphEmitMs = 0;
    static constexpr uint32_t morphEmitMinMs = 20;

    juce::TextButton modeMorph { "Morph" }, modePresets { "Presets" };
    juce::TextButton assignA { "Set A" }, assignB { "Set B" }, assignC { "Set C" }, assignD { "Set D" };
    juce::TextButton clearBtn { "Clear" };
    juce::Label hint { {}, "Select a library voice, Set A-D, drag the pad. Clear resets corners + marker." };
    juce::Rectangle<int> padBounds;

    juce::ListBox presetList { "morphPresets", this };
    juce::TextEditor presetName;
    juce::TextButton savePreset { "Save preset" }, loadPreset { "Load" }, deletePreset { "Delete" };
    MorphPresetStore* store = nullptr;
};

} // namespace fmlib
