#pragma once

#include "library/BrowserList.h"
#include "library/FavoritesStore.h"
#include "library/PatchEntry.h"
#include "library/RecentStore.h"
#include "library/TagStore.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace fmlib
{

class PatchBrowser : public juce::Component,
                     private juce::TableListBoxModel,
                     private juce::TextEditor::Listener,
                     private juce::KeyListener,
                     private juce::Timer
{
public:
    using LoadFn = std::function<void(const PatchEntry&, bool loadBank)>;
    using FavFn = std::function<void(uint64_t)>;
    using StatsFn = std::function<void(const BrowserStats&)>;
    using TagsChangedFn = std::function<void()>;

    PatchBrowser();
    ~PatchBrowser() override;

    void setEntries (std::vector<PatchEntry> entries, FavoritesStore* favorites, TagStore* tags = nullptr,
                     RecentStore* recent = nullptr);
    /** Bank file view (true) vs single-voice files only (false). */
    void setBankFileView (bool banks);
    void setGroupByBank (bool on);
    void setShowFileColumns (bool on);
    void setHideDuplicates (bool on);
    void setTooltipsEnabled (bool on);
    bool getBankFileView() const { return bankFileView; }
    bool getGroupByBank() const { return groupByBank; }
    bool getHideDuplicates() const { return hideDuplicates; }
    BrowserStats getStats() const { return lastStats; }

    std::optional<PatchEntry> getSelectedVoice() const;
    std::optional<PatchEntry> getDraggedVoice() const { return draggedVoice; }

    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void setLoadCallback (LoadFn fn) { onLoad = std::move (fn); }
    void setFavoriteToggleCallback (FavFn fn) { onFav = std::move (fn); }
    void setBankFileViewChanged (std::function<void(bool)> fn) { onBankFileViewChanged = std::move (fn); }
    void setOnListFocused (std::function<void()> fn) { onListFocused = std::move (fn); }
    void setOnStatsChanged (StatsFn fn) { onStatsChanged = std::move (fn); }
    void setOnTagsChanged (TagsChangedFn fn) { onTagsChanged = std::move (fn); }

    void jumpPrevBank();
    void jumpNextBank();

    juce::TableListBox& getTable() { return table; }

private:
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
    void cellClicked (int row, int columnId, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int columnId, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void textEditorTextChanged (juce::TextEditor&) override;
    juce::String getCellTooltip (int rowNumber, int columnId) override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;
    void sortOrderChanged (int newSortColumnId, bool isForwards) override;
    void timerCallback() override;
    void rebuildFiltered();
    void rebuildColumns();
    void updateStickyHeader();
    void updateBankChrome();
    void loadRow (int row, bool loadBank);
    bool toggleFavoriteOnSelection();
    int selectedVoiceRow() const;
    int nextSelectableRow (int from, int delta) const;
    bool moveSelectionBy (int delta);
    void editTagsForRow (int row);
    void applyColumnSort (std::vector<PatchEntry>& voices, bool keepBankGroups) const;
    int compareEntries (const PatchEntry& a, const PatchEntry& b) const;
    static juce::String folderColumnText (const PatchEntry& e);
    static bool isBankFileVoice (const PatchEntry& e) { return BrowserList::isBankFileVoice (e); }

    juce::TextEditor search;
    juce::ToggleButton favOnly { "Favorites" };
    juce::TextButton groupToggle { "Bank" };
    juce::TextButton prevBank { "< Bank" }, nextBank { "Bank >" };
    juce::Label stickyBank;
    juce::TableListBox table { "patches", this };
    std::vector<PatchEntry> all;
    std::vector<BrowserRow> rows;
    FavoritesStore* favStore = nullptr;
    TagStore* tagStore = nullptr;
    RecentStore* recentStore = nullptr;
    bool bankFileView = true;
    bool groupByBank = true;
    bool showFileColumns = false;
    bool hideDuplicates = false;
    bool tooltipsEnabled = true;
    int sortColumnId = 5; // Slot — natural 1..32 order in bank + group mode
    bool sortForwards = true;
    LoadFn onLoad;
    FavFn onFav;
    std::function<void(bool)> onBankFileViewChanged;
    std::function<void()> onListFocused;
    StatsFn onStatsChanged;
    TagsChangedFn onTagsChanged;
    BrowserStats lastStats;
    int lastSentRow = -1;
    std::optional<PatchEntry> draggedVoice;
};

} // namespace fmlib
