#pragma once

#include "library/DeviceBuffer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace fmlib
{

class DeviceBufferPanel : public juce::Component,
                          public juce::DragAndDropTarget
{
public:
    using ActionFn = std::function<void()>;
    using LoadFn = std::function<void(const PatchEntry&)>;
    using StatusFn = std::function<void(const juce::String&)>;

    DeviceBufferPanel();
    ~DeviceBufferPanel() override = default;

    void setBuffer (DeviceBuffer* buffer);
    void refreshList();
    void resized() override;
    void paintOverChildren (juce::Graphics& g) override;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;
    /** Clears drag-silence state when a DnD operation finishes (drop or cancel). */
    void notifyDragEnded();

    ActionFn onRequest1, onRequest32, onSave, onClear, onSend;
    LoadFn onLoadVoice;
    StatusFn onStatus;
    std::function<std::optional<PatchEntry>()> onQueryDragVoice;
    std::function<void()> onListFocused;
    std::function<void(int corner0to3, const PatchEntry&)> onAssignMorphCorner;
    std::function<void(const PatchEntry&)> onAuditionVoice;

    juce::ListBox& getList() { return list; }

private:
    DeviceBuffer* buf = nullptr;
    juce::Label title { {}, "Device buffer" };
    juce::TextButton req1 { "Get 1" }, req32 { "Get 32" };
    juce::TextButton send { "Send" }, save { "Save..." }, clear { "Clear" };
    juce::ListBox list { "device", nullptr };
    int lastSentRow = -1;
    int dropHighlightRow = -1;
    int dragSourceRow = -1;
    bool dragOver = false;
    /** When true, selection changes must not SysEx-load (drag / silent select). */
    bool suppressLoad = false;

    class Model : public juce::ListBoxModel
    {
    public:
        DeviceBufferPanel* owner = nullptr;
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;
        juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;
    } model;

    void loadRow (int row);
    void selectRowSilent (int row);
    void showVoiceContextMenu (int row);
    int rowAtDrag (const SourceDetails& details) const;
    bool isLibraryVoiceDrag (const SourceDetails& details) const;
    bool isReorderDrag (const SourceDetails& details) const;
    void reportStatus (const juce::String& s);
};

} // namespace fmlib
