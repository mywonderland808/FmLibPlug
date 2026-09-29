#pragma once

#include "library/BrowserList.h"
#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include "library/RecentStore.h"
#include "library/TagStore.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

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
    /** Resolve full PatchEntry (with VoiceData) from browse metadata. */
    using ResolveFn = std::function<std::optional<PatchEntry>(const PatchMeta&)>;

    PatchBrowser();
    ~PatchBrowser() override;

    void setEntries (std::vector<PatchMeta> entries, FavoritesStore* favorites, TagStore* tags = nullptr,
                     RecentStore* recent = nullptr);
    void setResolveCallback (ResolveFn fn) { onResolve = std::move (fn); }
    /** Bank file view (true) vs All voices flat (false). */
    void setBankFileView (bool banks);
    void setShowFileColumns (bool on);
    void setHideDuplicates (bool on);
    void setTooltipsEnabled (bool on);
    void setFavoritesOnly (bool on);
    void setTagFilterExpanded (bool on);

    std::optional<PatchEntry> getSelectedVoice() const;
    std::optional<PatchEntry> getDraggedVoice() const { return draggedVoice; }

    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void setLoadCallback (LoadFn fn) { onLoad = std::move (fn); }
    void setFavoriteToggleCallback (FavFn fn) { onFav = std::move (fn); }
    void setBankFileViewChanged (std::function<void(bool)> fn) { onBankFileViewChanged = std::move (fn); }
    void setOnAssignMorphCorner (std::function<void(int corner0to3, const PatchEntry&)> fn)
    {
        onAssignMorphCorner = std::move (fn);
    }
    void setOnAuditionVoice (std::function<void(const PatchEntry&)> fn) { onAuditionVoice = std::move (fn); }
    void setOnFavoritesOnlyChanged (std::function<void(bool)> fn) { onFavoritesOnlyChanged = std::move (fn); }
    void setOnTagFilterExpandedChanged (std::function<void(bool)> fn) { onTagFilterExpandedChanged = std::move (fn); }
    void setOnListFocused (std::function<void()> fn) { onListFocused = std::move (fn); }
    void setOnStatsChanged (StatsFn fn) { onStatsChanged = std::move (fn); }
    void setOnTagsChanged (TagsChangedFn fn) { onTagsChanged = std::move (fn); }

    void jumpPrevBank();
    void jumpNextBank();
    void refreshTagStrip();

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
    void mouseDown (const juce::MouseEvent& e) override;
    void rebuildFiltered();
    void applyRebuildResult (uint64_t generation,
                             std::vector<BrowserRow> nextRows,
                             BrowserStats stats,
                             std::optional<PatchMeta> keepSelected);
    void rebuildColumns();
    void updateStickyHeader();
    void updateBankChrome();
    void updateListToggleUi();
    void applyDefaultSortForCurrentView();
    BrowserScope currentScope() const;
    enum class LoadSource { selectionChange, reclick, bankLoad };
    void requestLoad (int row, bool loadBank, LoadSource source);
    bool toggleFavoriteOnSelection();
    int selectedVoiceRow() const;
    int nextSelectableRow (int from, int delta) const;
    bool moveSelectionBy (int delta);
    bool handleJumpKey (const juce::KeyPress& key);
    bool jumpKeysEnabled() const;
    void editTagsForRow (int row);
    void showVoiceContextMenu (int row);
    void showSearchFilterMenu();
    void insertSearchFilterToken (const juce::String& token);
    void toggleTagInSearch (const std::string& tagName,
                            LibraryFilter::TagChipCombine combine = LibraryFilter::TagChipCombine::replace);
    void clearSearch();
    /** Lay out tag chips for width; updates tagStrip size. */
    void rebuildTagStripButtons (int forWidth = 0);
    void updateTagFilterHeader();
    std::vector<std::string> tagFilterCatalog() const;
    const std::vector<std::string>& tagsForDisplay (const PatchMeta& m) const;
    /** Returns tag name under local cell point, or empty if none. */
    std::string tagAtCellPoint (int row, int width, int height, juce::Point<float> local) const;
    static juce::String folderColumnText (const PatchMeta& m);
    /** Lazy full entry via onResolve (libraryIndex / path+slot). */
    std::optional<PatchEntry> resolveEntry (const PatchMeta& m) const;

    class SearchField : public juce::TextEditor
    {
    public:
        std::function<void()> onShowFilterMenu;
        void mouseDown (const juce::MouseEvent& e) override
        {
            juce::TextEditor::mouseDown (e);
            // Left-click places the caret, then offers filter tokens; right-click keeps cut/copy/paste.
            if (e.mods.isLeftButtonDown() && onShowFilterMenu != nullptr)
                onShowFilterMenu();
        }
    };

    SearchField search;
    juce::TextButton clearSearchBtn { "Clear search" };
    juce::TextButton favOnly { "Favorites" };
    juce::TextButton groupToggle { "Bank" };
    juce::TextButton prevBank { "< Bank" }, nextBank { "Bank >" };
    juce::Label stickyBank;
    juce::TextButton tagFilterToggle { "Show Tags" };
    juce::Label tagFilterSummary;
    juce::Viewport tagStripViewport;
    juce::Component tagStrip;
    juce::OwnedArray<juce::TextButton> tagStripButtons;
    bool tagFilterExpanded = false;
    bool tagFilterHasCatalog = false;
    juce::TableListBox table { "patches", this };
    std::shared_ptr<const std::vector<PatchMeta>> all
        = std::make_shared<const std::vector<PatchMeta>>();
    std::vector<BrowserRow> rows;
    FavoritesStore* favStore = nullptr;
    TagStore* tagStore = nullptr;
    RecentStore* recentStore = nullptr;
    bool bankFileView = true;
    bool showFileColumns = false;
    bool hideDuplicates = false;
    bool tooltipsEnabled = true;
    int sortColumnId = 5; // Slot in Bank view; setBankFileView switches All to Patch (2)
    bool sortForwards = true;
    LoadFn onLoad;
    FavFn onFav;
    ResolveFn onResolve;
    std::function<void(bool)> onBankFileViewChanged;
    std::function<void(int, const PatchEntry&)> onAssignMorphCorner;
    std::function<void(const PatchEntry&)> onAuditionVoice;
    std::function<void(bool)> onFavoritesOnlyChanged;
    std::function<void(bool)> onTagFilterExpandedChanged;
    std::function<void()> onListFocused;
    StatsFn onStatsChanged;
    TagsChangedFn onTagsChanged;
    BrowserStats lastStats;
    int lastSentRow = -1;
    /** Row selected before the current mouse interaction (for re-click vs new-select). */
    int selectedRowBeforeClick = -1;
    /** Rebuild / silent reselect must not SysEx-load. */
    bool suppressLoad = false;
    std::optional<PatchEntry> draggedVoice;

    std::atomic<uint64_t> rebuildGeneration { 0 };
    struct RebuildCoordinator;
    std::shared_ptr<RebuildCoordinator> rebuildCoord;
    std::thread rebuildWorker;

    void joinRebuildWorker();
};

} // namespace fmlib
