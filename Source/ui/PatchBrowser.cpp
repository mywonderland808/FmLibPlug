#include "ui/PatchBrowser.h"
#include "library/LibraryFilter.h"
#include "ui/ThemePalette.h"
#include "util/StringUtils.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_set>

namespace fmlib
{

namespace
{
struct TagChip
{
    std::string name;
    juce::Rectangle<float> bounds;
};

std::vector<TagChip> layoutTagChips (const std::vector<std::string>& tags,
                                     const juce::Font& font,
                                     float width,
                                     float height)
{
    std::vector<TagChip> chips;
    constexpr float padX = 4.0f;
    constexpr float gap = 4.0f;
    constexpr float chipH = 18.0f;
    float x = padX;
    const float y = juce::jmax (0.0f, (height - chipH) * 0.5f);
    for (const auto& t : tags)
    {
        if (t.empty())
            continue;
        const auto label = juce::String::fromUTF8 (t.c_str());
        const float tw = juce::GlyphArrangement::getStringWidth (font, label) + 10.0f;
        if (x > padX && x + tw > width - padX)
            break;
        chips.push_back ({ t, { x, y, tw, chipH } });
        x += tw + gap;
    }
    return chips;
}
} // namespace

struct PatchBrowser::RebuildCoordinator
{
    std::atomic<bool> alive { true };
    std::atomic<bool> running { false };
    std::atomic<bool> pending { false };
};

PatchBrowser::PatchBrowser()
    : rebuildCoord (std::make_shared<RebuildCoordinator>())
{
    search.setTextToShowWhenEmpty ("Search... (click for filters)", juce::Colours::grey);
    search.setTooltip ("Type to filter. Left-click inserts filter tokens; right-click for cut/copy/paste.");
    search.onShowFilterMenu = [this] { showSearchFilterMenu(); };
    addAndMakeVisible (search);
    search.addListener (this);

    clearSearchBtn.setTooltip ("Clear the search field (Favorites toggle is unchanged)");
    clearSearchBtn.setButtonText ("Clear search");
    clearSearchBtn.onClick = [this] { clearSearch(); };
    addAndMakeVisible (clearSearchBtn);

    favOnly.setClickingTogglesState (true);
    favOnly.setTooltip ("Show favorites only");
    favOnly.onClick = [this]
    {
        if (onFavoritesOnlyChanged)
            onFavoritesOnlyChanged (favOnly.getToggleState());
        rebuildFiltered();
    };
    addAndMakeVisible (favOnly);

    groupToggle.setClickingTogglesState (true);
    groupToggle.setToggleState (true, juce::dontSendNotification);
    groupToggle.setButtonText ("Bank");
    groupToggle.onClick = [this]
    {
        bankFileView = groupToggle.getToggleState();
        updateListToggleUi();
        applyDefaultSortForCurrentView();
        if (onBankFileViewChanged)
            onBankFileViewChanged (bankFileView);
        updateBankChrome();
        rebuildFiltered();
    };
    addAndMakeVisible (groupToggle);
    updateListToggleUi();

    prevBank.onClick = [this] { jumpPrevBank(); };
    nextBank.onClick = [this] { jumpNextBank(); };
    addAndMakeVisible (prevBank);
    addAndMakeVisible (nextBank);

    tagFilterToggle.setClickingTogglesState (true);
    tagFilterToggle.setTooltip ("Show or hide the multi-line tag filter");
    tagFilterToggle.onClick = [this]
    {
        tagFilterExpanded = tagFilterToggle.getToggleState();
        updateTagFilterHeader();
        resized();
        if (onTagFilterExpandedChanged)
            onTagFilterExpandedChanged (tagFilterExpanded);
    };
    addAndMakeVisible (tagFilterToggle);
    updateTagFilterHeader();

    tagFilterSummary.setJustificationType (juce::Justification::centredLeft);
    tagFilterSummary.setFont (juce::FontOptions (12.0f));
    tagFilterSummary.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (tagFilterSummary);

    tagStripViewport.setViewedComponent (&tagStrip, false);
    tagStripViewport.setScrollBarsShown (true, false);
    tagStripViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible (tagStripViewport);

    stickyBank.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    stickyBank.setJustificationType (juce::Justification::centredLeft);
    stickyBank.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (stickyBank);

    table.setHeaderHeight (24);
    rebuildColumns();
    table.setMultipleSelectionEnabled (false);
    table.setWantsKeyboardFocus (true);
    table.addKeyListener (this);
    addAndMakeVisible (table);
    setWantsKeyboardFocus (true);
    updateBankChrome();
    table.addMouseListener (this, true);
}

PatchBrowser::~PatchBrowser()
{
    table.removeMouseListener (this);
    table.removeKeyListener (this);
    if (rebuildCoord != nullptr)
        rebuildCoord->alive = false;
    ++rebuildGeneration;
    if (rebuildWorker.joinable())
        rebuildWorker.detach();
}

void PatchBrowser::joinRebuildWorker()
{
    if (rebuildWorker.joinable())
        rebuildWorker.join();
    if (rebuildCoord != nullptr)
        rebuildCoord->running = false;
}

void PatchBrowser::rebuildColumns()
{
    auto& h = table.getHeader();
    h.removeAllColumns();

    constexpr auto flags = juce::TableHeaderComponent::defaultFlags;
    constexpr auto fixedStar = juce::TableHeaderComponent::notResizable;

    h.addColumn ("", 1, 28, 28, 28, fixedStar);
    h.addColumn ("Patch", 2, 220, 80, -1, flags);
    h.addColumn ("Slot", 5, 48, 40, 72, flags);
    h.addColumn ("Tags", 6, 160, 60, -1, flags);
    if (showFileColumns)
    {
        h.addColumn ("File", 3, 140, 60, -1, flags);
        h.addColumn ("Folder", 4, 160, 60, -1, flags);
    }

    h.setStretchToFitActive (false);
    h.setSortColumnId (sortColumnId, sortForwards);
    table.updateContent();
    table.repaint();
}

void PatchBrowser::setBankFileView (bool banks)
{
    if (bankFileView == banks)
    {
        groupToggle.setToggleState (banks, juce::dontSendNotification);
        updateListToggleUi();
        return;
    }
    bankFileView = banks;
    groupToggle.setToggleState (banks, juce::dontSendNotification);
    updateListToggleUi();
    applyDefaultSortForCurrentView();
    updateBankChrome();
    rebuildFiltered();
}

void PatchBrowser::setShowFileColumns (bool on)
{
    if (showFileColumns == on)
        return;
    showFileColumns = on;
    rebuildColumns();
}

void PatchBrowser::setHideDuplicates (bool on)
{
    if (hideDuplicates == on)
        return;
    hideDuplicates = on;
    rebuildFiltered();
}

void PatchBrowser::setTooltipsEnabled (bool on)
{
    tooltipsEnabled = on;
}

void PatchBrowser::setFavoritesOnly (bool on)
{
    if (favOnly.getToggleState() == on)
        return;
    favOnly.setToggleState (on, juce::dontSendNotification);
    rebuildFiltered();
}

void PatchBrowser::updateListToggleUi()
{
    if (bankFileView)
    {
        groupToggle.setButtonText ("Bank");
        groupToggle.setTooltip (
            "Bank view: voices from multi-voice bank files, always sorted and grouped by bank file name.");
    }
    else
    {
        groupToggle.setButtonText ("All");
        groupToggle.setTooltip (
            "All view: every library preset in a flat list, including 1-voice SysEx and "
            "voices from bank files. Filter :singles for 1-voice SysEx (not bank slots). Switch to Bank for grouped banks.");
    }
}

BrowserScope PatchBrowser::currentScope() const
{
    return bankFileView ? BrowserScope::bankFiles : BrowserScope::allVoices;
}

void PatchBrowser::applyDefaultSortForCurrentView()
{
    // Bank: slot order. All: Patch name (A-Z jump + browsing).
    sortColumnId = bankFileView ? 5 : 2;
    sortForwards = true;
    table.getHeader().setSortColumnId (sortColumnId, sortForwards);
}

void PatchBrowser::updateBankChrome()
{
    stickyBank.setVisible (bankFileView);

    if (bankFileView)
    {
        prevBank.setVisible (true);
        nextBank.setVisible (true);
        prevBank.setEnabled (true);
        nextBank.setEnabled (true);
        prevBank.setButtonText ("< Bank");
        nextBank.setButtonText ("Bank >");
        prevBank.setTooltip ("Previous bank. Keyboard: Left arrow.");
        nextBank.setTooltip ("Next bank. Keyboard: Right arrow.");
    }
    else if (sortColumnId == 2) // Patch (name)
    {
        prevBank.setVisible (true);
        nextBank.setVisible (true);
        prevBank.setEnabled (true);
        nextBank.setEnabled (true);
        prevBank.setButtonText ("< A-Z");
        nextBank.setButtonText ("A-Z >");
        prevBank.setTooltip ("Previous name group. Keyboard: Left arrow. Letter groups A-Z; numbers and symbols share one group.");
        nextBank.setTooltip ("Next name group. Keyboard: Right arrow. Letter groups A-Z; numbers and symbols share one group.");
    }
    else
    {
        // Name-group jump is only meaningful when the list is sorted by Patch name.
        prevBank.setVisible (true);
        nextBank.setVisible (true);
        prevBank.setEnabled (false);
        nextBank.setEnabled (false);
        prevBank.setButtonText ("< A-Z");
        nextBank.setButtonText ("A-Z >");
        prevBank.setTooltip ("Sort by Patch name to jump A-Z groups. Keyboard: Left arrow.");
        nextBank.setTooltip ("Sort by Patch name to jump A-Z groups. Keyboard: Right arrow.");
    }
    resized();
}

void PatchBrowser::setEntries (std::vector<PatchMeta> entries, FavoritesStore* favorites, TagStore* tags,
                               RecentStore* recent)
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].libraryIndex < 0)
            entries[i].libraryIndex = static_cast<int> (i);
        if (entries[i].voiceNameLower.empty() || entries[i].nameSortKey.empty())
            entries[i].refreshSearchCache();
    }
    all = std::make_shared<const std::vector<PatchMeta>> (std::move (entries));
    favStore = favorites;
    tagStore = tags;
    recentStore = recent;
    rebuildFiltered();
    updateStickyHeader();
    refreshTagStrip();
}

std::optional<PatchEntry> PatchBrowser::getSelectedVoice() const
{
    const int row = selectedVoiceRow();
    if (row < 0)
        return std::nullopt;
    return resolveEntry (rows[static_cast<size_t> (row)].meta);
}

std::optional<PatchEntry> PatchBrowser::resolveEntry (const PatchMeta& m) const
{
    if (onResolve)
        return onResolve (m);
    return std::nullopt;
}

void PatchBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto top = r.removeFromTop (28);
    // Always reserve bank-nav width so All mode does not reflow the search row.
    nextBank.setBounds (top.removeFromRight (64).reduced (1));
    prevBank.setBounds (top.removeFromRight (64).reduced (1));
    groupToggle.setBounds (top.removeFromRight (72));
    search.setBounds (top.reduced (0, 2));
    r.removeFromTop (4);

    {
        auto header = r.removeFromTop (24);
        favOnly.setBounds (header.removeFromLeft (90).reduced (1));
        clearSearchBtn.setBounds (header.removeFromLeft (96).reduced (1));
        clearSearchBtn.setEnabled (search.getText().isNotEmpty());
        favOnly.setVisible (true);
        clearSearchBtn.setVisible (true);

        if (tagFilterHasCatalog)
        {
            tagFilterToggle.setBounds (header.removeFromLeft (96).reduced (1));
            tagFilterSummary.setBounds (header.reduced (4, 0));
            tagFilterToggle.setVisible (true);
            tagFilterSummary.setVisible (true);
        }
        else
        {
            tagFilterToggle.setVisible (false);
            tagFilterSummary.setVisible (false);
            tagFilterSummary.setBounds ({});
        }
    }

    if (tagFilterHasCatalog && tagFilterExpanded)
    {
        // Size the strip to the wrapped chips; only scroll if it would starve the table.
        constexpr int kMinTableH = 140;
        constexpr int kStickyReserve = 26;
        const int maxBody = juce::jmax (28, r.getHeight() - kStickyReserve - kMinTableH);
        rebuildTagStripButtons (r.getWidth());
        if (tagStrip.getHeight() > maxBody)
            rebuildTagStripButtons (juce::jmax (80, r.getWidth() - tagStripViewport.getScrollBarThickness()));

        auto tagArea = r.removeFromTop (juce::jmin (tagStrip.getHeight(), maxBody));
        tagStripViewport.setBounds (tagArea);
        tagStripViewport.setVisible (true);
        tagStripViewport.setScrollBarsShown (tagStrip.getHeight() > tagStripViewport.getHeight(), false);
        r.removeFromTop (4);
    }
    else
    {
        tagStripViewport.setBounds ({});
        tagStripViewport.setVisible (false);
        if (tagFilterHasCatalog)
            r.removeFromTop (4);
    }

    // Always reserve sticky-header height so Bank <-> All does not reflow the table.
    auto stickyArea = r.removeFromTop (24);
    r.removeFromTop (2);
    stickyBank.setBounds (stickyArea);
    stickyBank.setVisible (bankFileView);
    table.setBounds (r);
}

std::vector<std::string> PatchBrowser::tagFilterCatalog() const
{
    return tagStore != nullptr ? tagStore->allUniqueTags() : std::vector<std::string> {};
}

const std::vector<std::string>& PatchBrowser::tagsForDisplay (const PatchMeta& m) const
{
    static const std::vector<std::string> empty;
    return tagStore != nullptr ? tagStore->getTags (m.contentId) : empty;
}

void PatchBrowser::refreshTagStrip()
{
    // Runs on the collapsed path too, where rebuildTagStripButtons never fires:
    // without this the Show Tags toggle would stay hidden once tags exist.
    tagFilterHasCatalog = ! tagFilterCatalog().empty();

    updateTagFilterHeader();
    resized();
}

void PatchBrowser::setTagFilterExpanded (bool on)
{
    tagFilterExpanded = on;
    tagFilterToggle.setToggleState (on, juce::dontSendNotification);
    updateTagFilterHeader();
    resized();
}

void PatchBrowser::updateTagFilterHeader()
{
    tagFilterToggle.setToggleState (tagFilterExpanded, juce::dontSendNotification);
    tagFilterToggle.setButtonText ("Show Tags");

    int activeCount = 0;
    const auto q = LibraryFilter::parse (search.getText().toStdString(), favOnly.getToggleState());
    for (const auto& group : q.orGroups)
        for (const auto& atom : group.atoms)
            if (atom.kind == LibraryFilterAtom::Kind::tag)
                ++activeCount;

    if (! tagFilterHasCatalog)
        tagFilterSummary.setText ({}, juce::dontSendNotification);
    else if (activeCount > 0)
        tagFilterSummary.setText (juce::String (activeCount) + (activeCount == 1 ? " tag selected" : " tags selected"),
                                  juce::dontSendNotification);
    else
        tagFilterSummary.setText (tagFilterExpanded
                                      ? "Click = replace, Shift = AND, Ctrl/Cmd = OR"
                                      : "Show tags to filter",
                                  juce::dontSendNotification);
}

void PatchBrowser::rebuildTagStripButtons (int forWidth)
{
    tagStripButtons.clear();
    tagStrip.removeAllChildren();

    const auto catalog = tagFilterCatalog();
    tagFilterHasCatalog = ! catalog.empty();
    if (! tagFilterHasCatalog)
    {
        tagStrip.setSize (0, 0);
        return;
    }

    const auto q = LibraryFilter::parse (search.getText().toStdString(), favOnly.getToggleState());
    std::unordered_set<std::string> active;
    for (const auto& group : q.orGroups)
        for (const auto& atom : group.atoms)
            if (atom.kind == LibraryFilterAtom::Kind::tag)
                active.insert (atom.value);

    constexpr int pad = 4;
    constexpr int gap = 4;
    constexpr int h = 22;
    const int viewW = juce::jmax (80, forWidth > 0 ? forWidth
                                                   : (tagStripViewport.getWidth() > 0 ? tagStripViewport.getWidth()
                                                                                     : getWidth()));
    int x = pad;
    int y = pad;
    int rowH = h;

    for (const auto& name : catalog)
    {
        auto* b = tagStripButtons.add (new juce::TextButton (juce::String::fromUTF8 (name.c_str())));
        b->setClickingTogglesState (true);
        const auto lower = juce::String::fromUTF8 (name.c_str()).toLowerCase().toStdString();
        b->setToggleState (active.count (lower) > 0, juce::dontSendNotification);
        b->setTooltip ("Click: filter by this tag. Shift: AND. Ctrl or Cmd: OR. Click again to remove. tag:"
                       + juce::String::fromUTF8 (name.c_str()));
        const int tw = juce::jmax (44, b->getBestWidthForHeight (h));
        if (x > pad && x + tw > viewW - pad)
        {
            x = pad;
            y += rowH + gap;
        }
        b->setBounds (x, y, tw, h);
        x += tw + gap;
        b->onClick = [safe = juce::Component::SafePointer<PatchBrowser> (this), name]
        {
            // Capture modifiers before async - they are gone by the time the message runs.
            const auto mods = juce::ModifierKeys::getCurrentModifiers();
            using Combine = LibraryFilter::TagChipCombine;
            Combine combine = Combine::replace;
            // Ctrl/Cmd wins over Shift so OR stays reachable when both are held.
            if (mods.isCommandDown() || mods.isCtrlDown())
                combine = Combine::withOr;
            else if (mods.isShiftDown())
                combine = Combine::withAnd;
            juce::MessageManager::callAsync ([safe, name, combine]
            {
                if (safe != nullptr)
                    safe->toggleTagInSearch (name, combine);
            });
        };
        tagStrip.addAndMakeVisible (b);
    }

    tagStrip.setSize (viewW, y + rowH + pad);
}

int PatchBrowser::nextSelectableRow (int from, int delta) const
{
    int row = from + delta;
    while (juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
    {
        if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::voice)
            return row;
        row += delta;
    }
    return -1;
}

bool PatchBrowser::moveSelectionBy (int delta)
{
    const int cur = table.getSelectedRow();
    const int start = cur >= 0 ? cur : (delta > 0 ? -1 : static_cast<int> (rows.size()));
    const int next = nextSelectableRow (start, delta);
    if (next < 0)
        return true;
    table.selectRow (next, false, true);
    updateStickyHeader();
    return true;
}

bool PatchBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::upKey))
        return moveSelectionBy (-1);
    if (key.isKeyCode (juce::KeyPress::downKey))
        return moveSelectionBy (1);
    if (key.isKeyCode (juce::KeyPress::pageUpKey)
        || key.isKeyCode (juce::KeyPress::pageDownKey)
        || key.isKeyCode (juce::KeyPress::homeKey)
        || key.isKeyCode (juce::KeyPress::endKey))
    {
        if (! table.hasKeyboardFocus (true))
            table.grabKeyboardFocus();
        const bool handled = table.keyPressed (key);
        if (handled)
        {
            const int row = table.getSelectedRow();
            if (juce::isPositiveAndBelow (row, static_cast<int> (rows.size()))
                && rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
            {
                const int next = nextSelectableRow (row, key.isKeyCode (juce::KeyPress::homeKey) ? 1 : -1);
                if (next >= 0)
                    table.selectRow (next, false, true);
            }
            updateStickyHeader();
        }
        return handled;
    }

    if (handleJumpKey (key))
        return true;

    if (key.isKeyCode (juce::KeyPress::returnKey))
        return toggleFavoriteOnSelection();

    return false;
}

bool PatchBrowser::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key.isKeyCode (juce::KeyPress::upKey))
        return moveSelectionBy (-1);
    if (key.isKeyCode (juce::KeyPress::downKey))
        return moveSelectionBy (1);
    if (handleJumpKey (key))
        return true;
    if (key.isKeyCode (juce::KeyPress::returnKey))
        return toggleFavoriteOnSelection();
    return false;
}

bool PatchBrowser::handleJumpKey (const juce::KeyPress& key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;
    if (! jumpKeysEnabled())
        return false;
    if (key.isKeyCode (juce::KeyPress::leftKey))
    {
        jumpPrevBank();
        return true;
    }
    if (key.isKeyCode (juce::KeyPress::rightKey))
    {
        jumpNextBank();
        return true;
    }
    return false;
}

bool PatchBrowser::jumpKeysEnabled() const
{
    return bankFileView || sortColumnId == 2;
}

int PatchBrowser::selectedVoiceRow() const
{
    const int row = table.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return -1;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return -1;
    return row;
}

bool PatchBrowser::toggleFavoriteOnSelection()
{
    const int row = selectedVoiceRow();
    if (row < 0 || ! onFav)
        return false;
    onFav (rows[static_cast<size_t> (row)].meta.contentId);
    if (favOnly.getToggleState())
        rebuildFiltered();
    else
        table.repaintRow (row);
    return true;
}

void PatchBrowser::jumpPrevBank()
{
    if (! jumpKeysEnabled())
        return;
    const auto next = bankFileView ? BrowserList::prevBankRow (rows, table.getSelectedRow())
                                   : BrowserList::prevNameGroupRow (rows, table.getSelectedRow());
    if (next)
    {
        table.selectRow (*next, false, true);
        updateStickyHeader();
    }
}

void PatchBrowser::jumpNextBank()
{
    if (! jumpKeysEnabled())
        return;
    const auto next = bankFileView ? BrowserList::nextBankRow (rows, table.getSelectedRow())
                                   : BrowserList::nextNameGroupRow (rows, table.getSelectedRow());
    if (next)
    {
        table.selectRow (*next, false, true);
        updateStickyHeader();
    }
}

void PatchBrowser::textEditorTextChanged (juce::TextEditor&)
{
    clearSearchBtn.setEnabled (search.getText().isNotEmpty());
    startTimer (120);
}

void PatchBrowser::timerCallback()
{
    stopTimer();
    rebuildFiltered();
    refreshTagStrip();
}

void PatchBrowser::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    sortColumnId = newSortColumnId;
    sortForwards = isForwards;
    updateBankChrome();
    rebuildFiltered();
}

void PatchBrowser::applyRebuildResult (uint64_t generation,
                                       std::vector<BrowserRow> nextRows,
                                       BrowserStats stats,
                                       std::optional<PatchMeta> keepSelected)
{
    if (generation != rebuildGeneration.load())
        return;

    rows = std::move (nextRows);
    lastSentRow = -1;
    lastStats = stats;
    lastStats.shown = BrowserList::countVoiceRows (rows);
    if (onStatsChanged)
        onStatsChanged (lastStats);

    suppressLoad = true;
    table.updateContent();
    if (keepSelected.has_value())
    {
        for (int i = 0; i < static_cast<int> (rows.size()); ++i)
        {
            if (rows[static_cast<size_t> (i)].kind != BrowserRowKind::voice)
                continue;
            const auto& e = rows[static_cast<size_t> (i)].meta;
            if (keepSelected->libraryIndex >= 0
                    ? e.libraryIndex == keepSelected->libraryIndex
                    : (e.absolutePath == keepSelected->absolutePath
                       && e.bankSlot == keepSelected->bankSlot
                       && e.contentId == keepSelected->contentId))
            {
                table.selectRow (i, false, false);
                lastSentRow = i;
                break;
            }
        }
    }
    suppressLoad = false;
    table.repaint();
    updateStickyHeader();
}

void PatchBrowser::rebuildFiltered()
{
    std::optional<PatchMeta> keepSelected;
    if (const int sel = selectedVoiceRow(); sel >= 0)
        keepSelected = rows[static_cast<size_t> (sel)].meta;

    auto q = LibraryFilter::parse (search.getText().toStdString(), favOnly.getToggleState());
    if (bankFileView && q.hasSingles())
    {
        bankFileView = false;
        groupToggle.setToggleState (false, juce::dontSendNotification);
        updateListToggleUi();
        applyDefaultSortForCurrentView();
        updateBankChrome();
        if (onBankFileViewChanged)
            onBankFileViewChanged (false);
        q = LibraryFilter::parse (search.getText().toStdString(), favOnly.getToggleState());
    }

    const auto scope = currentScope();
    const bool grouping = bankFileView && q.orGroups.empty() && ! q.duplicatesOnly;
    const bool hideDupes = hideDuplicates && ! q.duplicatesOnly;
    const int col = sortColumnId;
    const bool fwd = sortForwards;
    const auto allSnap = all;
    const auto recentIds = recentStore != nullptr ? recentStore->contentIds() : std::unordered_set<uint64_t> {};
    const bool needRecent = q.recentOnly && recentStore != nullptr;

    FavoritesStore favSnap;
    if (favStore != nullptr)
        favSnap = *favStore;
    std::optional<TagStore> tagSnap;
    if (tagStore != nullptr)
        tagSnap = *tagStore;

    const uint64_t jobGen = ++rebuildGeneration;
    joinRebuildWorker();

    const auto coord = rebuildCoord;
    juce::Component::SafePointer<PatchBrowser> safe (this);

    auto schedulePendingRebuild = [coord, safe]
    {
        if (coord == nullptr || ! coord->alive.load() || ! coord->pending.exchange (false))
            return;
        juce::MessageManager::callAsync ([safe, coord]
        {
            if (coord != nullptr && coord->alive.load() && safe != nullptr)
                safe->rebuildFiltered();
        });
    };

    // Small libraries: stay on the message thread (no thread hop flicker).
    if (allSnap->size() < 4000)
    {
        const auto* recentPtr = needRecent ? &recentIds : nullptr;
        const TagStore* tagsPtr = tagSnap.has_value() ? &*tagSnap : nullptr;
        auto filtered = BrowserList::filterForBrowser (*allSnap, scope, q, favSnap, tagsPtr, recentPtr);
        auto indices = std::move (filtered.voiceIndices);
        TagDisplayFn tagsOf;
        if (tagSnap.has_value())
        {
            tagsOf = [&tagSnap] (const PatchMeta& e) -> const std::string&
            {
                return tagSnap->displayJoined (e.contentId);
            };
        }
        BrowserList::applyColumnSort (*allSnap, indices, col, fwd, grouping, &favSnap, tagsOf);
        if (hideDupes)
            indices = LibraryFilter::keepFirstByContentId (*allSnap, std::move (indices));
        auto nextRows = BrowserList::buildRows (*allSnap, indices, grouping);
        applyRebuildResult (jobGen, std::move (nextRows), filtered.stats, std::move (keepSelected));
        schedulePendingRebuild();
        return;
    }

    if (coord == nullptr)
        return;

    coord->pending = true;
    if (coord->running.exchange (true))
        return;

    coord->pending = false;

    rebuildWorker = std::thread ([coord,
                                  safe,
                                  jobGen,
                                  allSnap,
                                  q,
                                  scope,
                                  grouping,
                                  hideDupes,
                                  col,
                                  fwd,
                                  favSnap = std::move (favSnap),
                                  tagSnap = std::move (tagSnap),
                                  recentIds,
                                  needRecent,
                                  keepSelected = std::move (keepSelected)]() mutable
    {
        struct RunningGuard
        {
            std::shared_ptr<RebuildCoordinator> c;
            ~RunningGuard()
            {
                if (c != nullptr)
                    c->running = false;
            }
        } guard { coord };

        if (coord == nullptr || ! coord->alive.load())
            return;

        const auto* recentPtr = needRecent ? &recentIds : nullptr;
        const TagStore* tagsPtr = tagSnap.has_value() ? &*tagSnap : nullptr;
        auto filtered = BrowserList::filterForBrowser (*allSnap, scope, q, favSnap, tagsPtr, recentPtr);
        auto indices = std::move (filtered.voiceIndices);
        TagDisplayFn tagsOf;
        if (tagSnap.has_value())
        {
            tagsOf = [&tagSnap] (const PatchMeta& e) -> const std::string&
            {
                return tagSnap->displayJoined (e.contentId);
            };
        }
        BrowserList::applyColumnSort (*allSnap, indices, col, fwd, grouping, &favSnap, tagsOf);
        if (hideDupes)
            indices = LibraryFilter::keepFirstByContentId (*allSnap, std::move (indices));
        auto nextRows = BrowserList::buildRows (*allSnap, indices, grouping);
        auto stats = filtered.stats;

        juce::MessageManager::callAsync ([coord, safe, jobGen, nextRows = std::move (nextRows), stats,
                                          keepSelected = std::move (keepSelected)]() mutable
        {
            if (coord == nullptr || ! coord->alive.load())
                return;
            if (safe != nullptr)
                safe->applyRebuildResult (jobGen, std::move (nextRows), stats, std::move (keepSelected));
            if (coord->alive.load() && coord->pending.exchange (false))
            {
                juce::MessageManager::callAsync ([safe, coord]
                {
                    if (coord != nullptr && coord->alive.load() && safe != nullptr)
                        safe->rebuildFiltered();
                });
            }
        });
    });
}

void PatchBrowser::updateStickyHeader()
{
    if (! bankFileView)
    {
        stickyBank.setText ({}, juce::dontSendNotification);
        return;
    }

    const bool dark = findColour (juce::ResizableWindow::backgroundColourId).getBrightness() < 0.5f;
    stickyBank.setColour (juce::Label::backgroundColourId, ThemePalette::forTheme (dark).stickyHeader);
    stickyBank.setColour (juce::Label::textColourId, findColour (juce::Label::textColourId));

    int row = table.getSelectedRow();
    if (row < 0)
        row = 0;
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
    {
        stickyBank.setText ({}, juce::dontSendNotification);
        return;
    }

    int i = row;
    while (i > 0 && rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i)].bankPath
                  == rows[static_cast<size_t> (i - 1)].bankPath)
        --i;
    if (i > 0 && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::sectionHeader)
        --i;

    if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::sectionHeader)
    {
        stickyBank.setText ("Bank: " + toJuce (rows[static_cast<size_t> (i)].sectionLabel)
                                + "  (" + juce::String (rows[static_cast<size_t> (i)].sectionVoiceCount) + " voices)",
                            juce::dontSendNotification);
    }
    else if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::voice)
    {
        stickyBank.setText ("Bank: " + toJuce (rows[static_cast<size_t> (row)].meta.fileName),
                            juce::dontSendNotification);
    }
}

int PatchBrowser::getNumRows()
{
    return static_cast<int> (rows.size());
}

void PatchBrowser::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    juce::ignoreUnused (width, height);
    if (juce::isPositiveAndBelow (row, static_cast<int> (rows.size()))
        && rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
    {
        const bool dark = findColour (juce::ResizableWindow::backgroundColourId).getBrightness() < 0.5f;
        g.fillAll (ThemePalette::forTheme (dark).stickyHeader);
        return;
    }
    g.fillAll (selected ? findColour (juce::TextEditor::highlightColourId).withAlpha (0.45f)
                         : findColour (juce::ListBox::backgroundColourId));
}

juce::String PatchBrowser::folderColumnText (const PatchMeta& m)
{
    if (! m.relativePath.empty())
    {
        const auto parent = std::filesystem::path (m.relativePath).parent_path();
        if (! parent.empty())
            return juce::String::fromUTF8 (parent.generic_string().c_str());
    }
    if (! m.baseFolder.empty())
        return juce::String::fromUTF8 (m.baseFolder.filename().string().c_str());
    return ".";
}

static void paintFavoriteStar (juce::Graphics& g, juce::Rectangle<float> bounds, bool filled, juce::Colour colour)
{
    const auto cx = bounds.getCentreX();
    const auto cy = bounds.getCentreY();
    const auto rOuter = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.38f;
    const auto rInner = rOuter * 0.45f;
    juce::Path star;
    for (int i = 0; i < 5; ++i)
    {
        const float tip = -juce::MathConstants<float>::halfPi
                          + (float) i * juce::MathConstants<float>::twoPi / 5.0f;
        const float dent = tip + juce::MathConstants<float>::twoPi / 10.0f;
        const float ox = cx + rOuter * std::cos (tip);
        const float oy = cy + rOuter * std::sin (tip);
        const float ix = cx + rInner * std::cos (dent);
        const float iy = cy + rInner * std::sin (dent);
        if (i == 0)
            star.startNewSubPath (ox, oy);
        else
            star.lineTo (ox, oy);
        star.lineTo (ix, iy);
    }
    star.closeSubPath();
    g.setColour (colour);
    if (filled)
        g.fillPath (star);
    else
        g.strokePath (star, juce::PathStrokeType (1.0f));
}

void PatchBrowser::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;
    const auto& r = rows[static_cast<size_t> (row)];
    g.setColour (findColour (juce::Label::textColourId));

    if (r.kind == BrowserRowKind::sectionHeader)
    {
        if (columnId == 2)
        {
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (juce::String::fromUTF8 (r.sectionLabel.c_str()) + " (" + juce::String (r.sectionVoiceCount) + ")",
                        4, 0, width - 8, height, juce::Justification::centredLeft, true);
        }
        return;
    }

    const auto& e = r.meta;
    if (columnId == 1)
    {
        const bool fav = favStore != nullptr && favStore->isFavorite (e.contentId);
        const auto textColour = findColour (juce::Label::textColourId);
        const auto bgColour = findColour (juce::ListBox::backgroundColourId);
        // Unfilled stars stay quiet: mostly background, light outline only.
        const auto starColour = fav ? textColour : textColour.interpolatedWith (bgColour, 0.78f);
        paintFavoriteStar (g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height),
                           fav, starColour);
        return;
    }

    juce::String text;
    switch (columnId)
    {
        case 2: text = juce::String::fromUTF8 (e.voiceName.c_str()); break;
        case 3: text = juce::String::fromUTF8 (e.fileName.c_str()); break;
        case 4: text = folderColumnText (e); break;
        case 5: text = e.bankSlot > 0 ? juce::String (e.bankSlot) : ""; break;
        case 6:
        {
            const auto tags = tagsForDisplay (e);
            juce::Font font (juce::FontOptions (13.0f));
            g.setFont (font);
            const auto chips = layoutTagChips (tags, font, (float) width, (float) height);
            const auto textColour = findColour (juce::Label::textColourId);
            const auto bg = findColour (juce::TextEditor::backgroundColourId);
            for (const auto& chip : chips)
            {
                g.setColour (textColour.interpolatedWith (bg, 0.72f));
                g.fillRoundedRectangle (chip.bounds, 3.0f);
                g.setColour (textColour);
                g.drawText (juce::String::fromUTF8 (chip.name.c_str()),
                            chip.bounds.toNearestInt(),
                            juce::Justification::centred, true);
            }
            return;
        }
        default: break;
    }
    g.drawText (text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

juce::String PatchBrowser::getCellTooltip (int rowNumber, int columnId)
{
    if (! tooltipsEnabled)
        return {};
    if (! juce::isPositiveAndBelow (rowNumber, static_cast<int> (rows.size())))
        return {};
    if (columnId == 6)
        return "Click a tag to filter. Shift = AND, Ctrl or Cmd = OR, Alt = edit tags. Right-click a voice for more.";
    const auto& e = rows[static_cast<size_t> (rowNumber)].meta;
    return juce::String::fromUTF8 (e.fileName.c_str()) + " - "
         + juce::String::fromUTF8 (e.absolutePath.string().c_str());
}

juce::var PatchBrowser::getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() == 0)
        return {};
    const int row = rowsToDescribe[0];
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return {};
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return {};
    draggedVoice = resolveEntry (rows[static_cast<size_t> (row)].meta);
    return juce::var ("fmlib-voice");
}

void PatchBrowser::requestLoad (int row, bool loadBank, LoadSource source)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())) || ! onLoad)
        return;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return;
    if (source == LoadSource::selectionChange && ! loadBank && row == lastSentRow)
        return;
    auto entry = resolveEntry (rows[static_cast<size_t> (row)].meta);
    if (! entry.has_value())
        return;
    lastSentRow = row;
    onLoad (*entry, loadBank);
    updateStickyHeader();
}

void PatchBrowser::mouseDown (const juce::MouseEvent&)
{
    selectedRowBeforeClick = table.getSelectedRow();
}

void PatchBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (suppressLoad)
        return;
    // Row is selected on mouse-down, before cellClicked. Right-click must not SysEx-load.
    if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu())
        return;
    if (! juce::isPositiveAndBelow (lastRowSelected, static_cast<int> (rows.size())))
        return;
    if (rows[static_cast<size_t> (lastRowSelected)].kind == BrowserRowKind::sectionHeader)
    {
        const int next = nextSelectableRow (lastRowSelected, 1);
        if (next >= 0 && next != lastRowSelected)
            table.selectRow (next, false, true);
        return;
    }
    requestLoad (lastRowSelected, false, LoadSource::selectionChange);
}

void PatchBrowser::toggleTagInSearch (const std::string& tagName, LibraryFilter::TagChipCombine combine)
{
    const auto next = LibraryFilter::toggleAndTagToken (search.getText().toStdString(), tagName, combine);
    search.setText (juce::String::fromUTF8 (next.c_str()), juce::dontSendNotification);
    rebuildFiltered();
    refreshTagStrip();
}

void PatchBrowser::clearSearch()
{
    if (search.getText().isEmpty())
        return;
    search.clear();
    rebuildFiltered();
    refreshTagStrip();
}

std::string PatchBrowser::tagAtCellPoint (int row, int width, int height, juce::Point<float> local) const
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return {};
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return {};
    const auto tags = tagsForDisplay (rows[static_cast<size_t> (row)].meta);
    juce::Font font (juce::FontOptions (13.0f));
    const auto chips = layoutTagChips (tags, font, (float) width, (float) height);
    for (const auto& chip : chips)
        if (chip.bounds.contains (local))
            return chip.name;
    return {};
}

void PatchBrowser::editTagsForRow (int row)
{
    if (tagStore == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return;

    const auto id = rows[static_cast<size_t> (row)].meta.contentId;
    const auto name = toJuce (rows[static_cast<size_t> (row)].meta.voiceName);
    auto current = tagStore->getTags (id);
    const auto catalog = tagStore->allUniqueTags();

    auto* aw = new juce::AlertWindow ("Edit tags",
                                      "Tags for \"" + name + "\" (comma-separated):",
                                      juce::AlertWindow::NoIcon);
    themeAlertWindow (*aw, getLookAndFeel());
    juce::StringArray parts;
    for (const auto& t : current)
        parts.add (toJuce (t));
    aw->addTextEditor ("tags", parts.joinIntoString (", "), "Tags");

    if (! catalog.empty())
    {
        juce::StringArray items;
        items.add ("(add existing tag...)");
        for (const auto& t : catalog)
            items.add (juce::String::fromUTF8 (t.c_str()));
        aw->addComboBox ("addExisting", items, "Add existing");
        if (auto* box = aw->getComboBoxComponent ("addExisting"))
        {
            box->onChange = [aw, box]
            {
                const int idx = box->getSelectedItemIndex();
                if (idx <= 0)
                    return;
                const auto tag = box->getText().trim().toLowerCase();
                box->setSelectedItemIndex (0, juce::dontSendNotification);
                if (tag.isEmpty())
                    return;
                if (auto* ed = aw->getTextEditor ("tags"))
                {
                    juce::StringArray tokens;
                    tokens.addTokens (ed->getText(), ",;", "");
                    for (auto& t : tokens)
                        t = t.trim().toLowerCase();
                    tokens.removeEmptyStrings();
                    if (tokens.contains (tag))
                        return;
                    tokens.add (tag);
                    ed->setText (tokens.joinIntoString (", "));
                }
            };
        }
    }

    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->addButton ("Clear", 2);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, id] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (aw);
        aw->setLookAndFeel (nullptr);
        if (tagStore == nullptr || result == 0)
            return;

        std::vector<std::string> tags;
        if (result == 1)
        {
            auto text = aw->getTextEditorContents ("tags");
            juce::StringArray tokens;
            tokens.addTokens (text, ",;", "");
            for (auto& t : tokens)
            {
                t = t.trim().toLowerCase();
                if (t.isNotEmpty())
                    tags.push_back (t.toStdString());
            }
        }
        // result == 2 (Clear): empty tags
        tagStore->setTags (id, std::move (tags));
        if (onTagsChanged)
            onTagsChanged();
        table.repaint();
    }));
}

void PatchBrowser::showSearchFilterMenu()
{
    enum
    {
        fav = 1,
        recent,
        dupe,
        singles,
        tag,
        andOp,
        orOp,
        clearSearchItem,
        tagPickBase = 1000
    };

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("Filter tokens");
    menu.addItem (fav, "fav:  -  favorites only");
    menu.addItem (recent, "recent:  -  recently loaded voices");
    menu.addItem (dupe, "dupe:  -  voices that appear more than once");
    menu.addItem (singles, ":singles  -  1-voice SysEx (not bank slots)");
    menu.addItem (tag, "tag:  -  filter by tag (type name after)");

    menu.addSeparator();
    menu.addSectionHeader ("Tag filters");
    const auto catalog = tagFilterCatalog();
    {
        juce::PopupMenu tagMenu;
        if (catalog.empty())
        {
            tagMenu.addItem (-1, "(no tags yet)", false, false);
        }
        else
        {
            for (size_t i = 0; i < catalog.size(); ++i)
                tagMenu.addItem (tagPickBase + static_cast<int> (i),
                                 juce::String::fromUTF8 (catalog[i].c_str()));
        }
        menu.addSubMenu ("Pick existing tag", tagMenu);
    }

    menu.addSeparator();
    menu.addSectionHeader ("Combine");
    menu.addItem (andOp, "AND  -  require both sides (uppercase)");
    menu.addItem (orOp, "OR  -  match either side (uppercase)");
    menu.addSeparator();
    menu.addItem (clearSearchItem, "Clear search", search.getText().isNotEmpty());

    juce::Component::SafePointer<PatchBrowser> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&search),
                        [safe, catalog] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result >= tagPickBase
                                && result < tagPickBase + static_cast<int> (catalog.size()))
                            {
                                safe->toggleTagInSearch (catalog[static_cast<size_t> (result - tagPickBase)]);
                                return;
                            }
                            switch (result)
                            {
                                case fav: safe->insertSearchFilterToken ("fav:"); break;
                                case recent: safe->insertSearchFilterToken ("recent:"); break;
                                case dupe: safe->insertSearchFilterToken ("dupe:"); break;
                                case singles: safe->insertSearchFilterToken (":singles"); break;
                                case tag: safe->insertSearchFilterToken ("tag:"); break;
                                case andOp: safe->insertSearchFilterToken ("AND"); break;
                                case orOp: safe->insertSearchFilterToken ("OR"); break;
                                case clearSearchItem:
                                    safe->clearSearch();
                                    safe->search.grabKeyboardFocus();
                                    break;
                                default: break;
                            }
                        });
}

void PatchBrowser::insertSearchFilterToken (const juce::String& token)
{
    auto text = search.getText();
    int caret = juce::jlimit (0, text.length(), search.getCaretPosition());
    const bool isCombinator = token == "AND" || token == "OR";
    const auto low = text.toLowerCase();

    if (isCombinator)
    {
        if (text.trim().isEmpty())
            return;
    }
    else if (token == "fav:" && low.contains ("fav:"))
        return;
    else if (token == "recent:" && low.contains ("recent:"))
        return;
    else if (token == "dupe:" && (low.contains ("dupe:") || low.contains ("dup:")))
        return;
    else if (token == ":singles" && (low.contains (":singles") || low.contains ("singles:")))
        return;

    juce::String insert = token;
    if (caret > 0 && ! juce::CharacterFunctions::isWhitespace (text[caret - 1]))
        insert = " " + insert;
    if (! isCombinator && token == "tag:")
    {
        // leave caret after tag: for typing the name
    }
    else if (isCombinator)
    {
        if (caret < text.length() && ! juce::CharacterFunctions::isWhitespace (text[caret]))
            insert = insert + " ";
        else if (caret >= text.length())
            insert = insert + " ";
    }

    const auto next = text.substring (0, caret) + insert + text.substring (caret);
    const int newCaret = caret + insert.length();
    search.setText (next, juce::dontSendNotification);
    search.grabKeyboardFocus();
    search.setCaretPosition (newCaret);
    rebuildFiltered();
}

void PatchBrowser::showVoiceContextMenu (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return;

    enum
    {
        editTags = 1,
        openInFolder = 2,
        auditionVoice = 3,
        setCornerA = 10,
        setCornerB = 11,
        setCornerC = 12,
        setCornerD = 13
    };

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    const auto& meta = rows[static_cast<size_t> (row)].meta;
    if (onAuditionVoice)
        menu.addItem (auditionVoice, "Audition");
    if (onAssignMorphCorner)
    {
        juce::PopupMenu corners;
        corners.addItem (setCornerA, "A");
        corners.addItem (setCornerB, "B");
        corners.addItem (setCornerC, "C");
        corners.addItem (setCornerD, "D");
        menu.addSubMenu ("Set morph corner", corners);
    }
    if (onAuditionVoice || onAssignMorphCorner)
        menu.addSeparator();
    menu.addItem (editTags, "Edit tags");
    const juce::File file (juce::String::fromUTF8 (meta.absolutePath.string().c_str()));
    menu.addItem (openInFolder, "Open in folder", file.existsAsFile() || file.getParentDirectory().isDirectory());

    juce::Component::SafePointer<PatchBrowser> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safe, row, file] (int result)
                        {
                            if (safe == nullptr)
                                return;

                            if (result == editTags)
                                safe->editTagsForRow (row);
                            else if (result == openInFolder)
                            {
                                if (file.existsAsFile())
                                    file.revealToUser();
                                else if (file.getParentDirectory().isDirectory())
                                    file.getParentDirectory().revealToUser();
                            }
                            else if (result == auditionVoice && safe->onAuditionVoice)
                            {
                                if (auto entry = safe->resolveEntry (safe->rows[static_cast<size_t> (row)].meta))
                                    safe->onAuditionVoice (*entry);
                            }
                            else if (result >= setCornerA && result <= setCornerD && safe->onAssignMorphCorner)
                            {
                                if (auto entry = safe->resolveEntry (safe->rows[static_cast<size_t> (row)].meta))
                                    safe->onAssignMorphCorner (result - setCornerA, *entry);
                            }
                        });
}

void PatchBrowser::cellClicked (int row, int columnId, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;

    table.grabKeyboardFocus();
    if (onListFocused)
        onListFocused();

    if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
        return;

    if (e.mods.isPopupMenu())
    {
        suppressLoad = true;
        table.selectRow (row, false, true);
        suppressLoad = false;
        showVoiceContextMenu (row);
        return;
    }

    if (columnId == 1)
    {
        const auto& meta = rows[static_cast<size_t> (row)].meta;
        if (onFav)
            onFav (meta.contentId);
        if (favOnly.getToggleState())
            rebuildFiltered();
        else
            table.repaintRow (row);
        return;
    }

    if (columnId == 6)
    {
        // cellClicked MouseEvent is relative to the full row, not the Tags cell.
        const int colIndex = table.getHeader().getIndexOfColumnId (6, true);
        if (colIndex < 0)
            return;
        const auto colBounds = table.getHeader().getColumnPosition (colIndex);
        const auto local = juce::Point<float> (e.position.x - (float) colBounds.getX(),
                                               e.position.y);
        const auto hit = tagAtCellPoint (row, colBounds.getWidth(), table.getRowHeight(), local);
        if (! hit.empty())
        {
            if (e.mods.isAltDown())
            {
                editTagsForRow (row);
                return;
            }
            using Combine = LibraryFilter::TagChipCombine;
            Combine combine = Combine::replace;
            if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                combine = Combine::withOr;
            else if (e.mods.isShiftDown())
                combine = Combine::withAnd;
            toggleTagInSearch (hit, combine);
            return;
        }
        // Missed a chip - fall through to normal row click / re-send.
    }

    if (row == table.getSelectedRow())
    {
        // New selection: selectedRowsChanged already loaded. Re-click same row: re-send.
        if (selectedRowBeforeClick == row)
        {
            lastSentRow = -1;
            requestLoad (row, false, LoadSource::reclick);
        }
    }
}

void PatchBrowser::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;
    lastSentRow = -1;
    if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
    {
        if (row + 1 < static_cast<int> (rows.size()))
            requestLoad (row + 1, true, LoadSource::bankLoad);
        return;
    }
    requestLoad (row, true, LoadSource::bankLoad);
}

} // namespace fmlib
