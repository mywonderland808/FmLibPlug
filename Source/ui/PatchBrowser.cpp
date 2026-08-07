#include "ui/PatchBrowser.h"
#include "library/LibraryFilter.h"
#include "ui/ThemePalette.h"
#include "util/StringUtils.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <unordered_map>

namespace fmlib
{

PatchBrowser::PatchBrowser()
{
    search.setTextToShowWhenEmpty ("Search... tag:bass AND dark OR dupe:", juce::Colours::grey);
    addAndMakeVisible (search);
    search.addListener (this);

    favOnly.setClickingTogglesState (true);
    favOnly.onClick = [this] { rebuildFiltered(); };
    addAndMakeVisible (favOnly);

    groupToggle.setClickingTogglesState (true);
    groupToggle.setToggleState (true, juce::dontSendNotification);
    groupToggle.setButtonText ("Bank");
    groupToggle.setTooltip ("Bank = multi-voice bank files. Single = one-voice files only.");
    groupToggle.onClick = [this]
    {
        bankFileView = groupToggle.getToggleState();
        groupToggle.setButtonText (bankFileView ? "Bank" : "Single");
        if (onBankFileViewChanged)
            onBankFileViewChanged (bankFileView);
        updateBankChrome();
        rebuildFiltered();
    };
    addAndMakeVisible (groupToggle);

    prevBank.onClick = [this] { jumpPrevBank(); };
    nextBank.onClick = [this] { jumpNextBank(); };
    addAndMakeVisible (prevBank);
    addAndMakeVisible (nextBank);

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
}

PatchBrowser::~PatchBrowser()
{
    table.removeKeyListener (this);
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
    bankFileView = banks;
    groupToggle.setToggleState (banks, juce::dontSendNotification);
    groupToggle.setButtonText (banks ? "Bank" : "Single");
    updateBankChrome();
    rebuildFiltered();
}

void PatchBrowser::setGroupByBank (bool on)
{
    groupByBank = on;
    updateBankChrome();
    rebuildFiltered();
}

void PatchBrowser::setShowFileColumns (bool on)
{
    showFileColumns = on;
    rebuildColumns();
}

void PatchBrowser::setHideDuplicates (bool on)
{
    hideDuplicates = on;
    rebuildFiltered();
}

void PatchBrowser::setTooltipsEnabled (bool on)
{
    tooltipsEnabled = on;
}

void PatchBrowser::updateBankChrome()
{
    // Keep layout stable: always reserve button slots; only toggle visibility.
    prevBank.setVisible (bankFileView);
    nextBank.setVisible (bankFileView);
    stickyBank.setVisible (bankFileView && groupByBank);
    resized();
}

void PatchBrowser::setEntries (std::vector<PatchEntry> entries, FavoritesStore* favorites, TagStore* tags,
                               RecentStore* recent)
{
    uint64_t previousId = 0;
    if (lastSentRow >= 0 && lastSentRow < static_cast<int> (rows.size())
        && rows[static_cast<size_t> (lastSentRow)].kind == BrowserRowKind::voice)
        previousId = rows[static_cast<size_t> (lastSentRow)].entry.contentId;

    all = std::move (entries);
    for (auto& e : all)
        if (e.voiceNameLower.empty())
            e.refreshSearchCache();
    favStore = favorites;
    tagStore = tags;
    recentStore = recent;
    rebuildFiltered();

    if (previousId != 0)
    {
        for (int i = 0; i < static_cast<int> (rows.size()); ++i)
        {
            if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
                && rows[static_cast<size_t> (i)].entry.contentId == previousId)
            {
                lastSentRow = -1;
                table.selectRow (i, false, false);
                break;
            }
        }
    }
    updateStickyHeader();
}

std::optional<PatchEntry> PatchBrowser::getSelectedVoice() const
{
    const int row = selectedVoiceRow();
    if (row < 0)
        return std::nullopt;
    return rows[static_cast<size_t> (row)].entry;
}

void PatchBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto top = r.removeFromTop (28);
    // Always reserve bank-nav width so Single mode does not reflow the search row.
    nextBank.setBounds (top.removeFromRight (64).reduced (1));
    prevBank.setBounds (top.removeFromRight (64).reduced (1));
    groupToggle.setBounds (top.removeFromRight (72));
    favOnly.setBounds (top.removeFromRight (90));
    search.setBounds (top.reduced (0, 2));
    r.removeFromTop (4);
    // Always reserve sticky-header height so Bank <-> Single does not reflow the table.
    auto stickyArea = r.removeFromTop (24);
    r.removeFromTop (2);
    stickyBank.setBounds (stickyArea);
    stickyBank.setVisible (bankFileView && groupByBank);
    table.setBounds (r);
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
        || key.isKeyCode (juce::KeyPress::endKey)
        || key.isKeyCode (juce::KeyPress::leftKey)
        || key.isKeyCode (juce::KeyPress::rightKey))
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

    if (bankFileView && key.getTextCharacter() == '[')
    {
        jumpPrevBank();
        return true;
    }
    if (bankFileView && key.getTextCharacter() == ']')
    {
        jumpNextBank();
        return true;
    }

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
    if (bankFileView && key.getTextCharacter() == '[')
    {
        jumpPrevBank();
        return true;
    }
    if (bankFileView && key.getTextCharacter() == ']')
    {
        jumpNextBank();
        return true;
    }
    if (key.isKeyCode (juce::KeyPress::returnKey))
        return toggleFavoriteOnSelection();
    return false;
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
    onFav (rows[static_cast<size_t> (row)].entry.contentId);
    table.repaintRow (row);
    return true;
}

void PatchBrowser::jumpPrevBank()
{
    if (! bankFileView)
        return;
    const auto next = BrowserList::prevBankRow (rows, table.getSelectedRow());
    if (next)
    {
        table.selectRow (*next, false, true);
        updateStickyHeader();
    }
}

void PatchBrowser::jumpNextBank()
{
    if (! bankFileView)
        return;
    const auto next = BrowserList::nextBankRow (rows, table.getSelectedRow());
    if (next)
    {
        table.selectRow (*next, false, true);
        updateStickyHeader();
    }
}

void PatchBrowser::textEditorTextChanged (juce::TextEditor&)
{
    startTimer (120);
}

void PatchBrowser::timerCallback()
{
    stopTimer();
    rebuildFiltered();
}

int PatchBrowser::compareEntries (const PatchEntry& a, const PatchEntry& b) const
{
    auto nameOf = [] (const PatchEntry& e) -> const std::string&
    {
        return e.voiceNameLower.empty() ? e.voiceName : e.voiceNameLower;
    };
    auto fileOf = [] (const PatchEntry& e) -> const std::string&
    {
        return e.fileNameLower.empty() ? e.fileName : e.fileNameLower;
    };
    auto pathOf = [] (const PatchEntry& e) -> const std::string&
    {
        return e.relativePathLower.empty() ? e.relativePath : e.relativePathLower;
    };
    auto tagsOf = [this] (const PatchEntry& e) -> std::string
    {
        if (tagStore == nullptr)
            return {};
        const auto tags = tagStore->getTags (e.contentId);
        std::string s;
        for (size_t i = 0; i < tags.size(); ++i)
        {
            if (i > 0)
                s += ',';
            s += tags[i];
        }
        return s;
    };

    switch (sortColumnId)
    {
        case 1:
        {
            const bool fa = favStore != nullptr && favStore->isFavorite (a.contentId);
            const bool fb = favStore != nullptr && favStore->isFavorite (b.contentId);
            if (fa != fb)
                return fa ? -1 : 1;
            break;
        }
        case 3:
        {
            if (fileOf (a) < fileOf (b))
                return -1;
            if (fileOf (b) < fileOf (a))
                return 1;
            break;
        }
        case 4:
        {
            if (pathOf (a) < pathOf (b))
                return -1;
            if (pathOf (b) < pathOf (a))
                return 1;
            break;
        }
        case 5:
        {
            if (a.bankSlot < b.bankSlot)
                return -1;
            if (b.bankSlot < a.bankSlot)
                return 1;
            break;
        }
        case 6:
        {
            const auto ta = tagsOf (a);
            const auto tb = tagsOf (b);
            if (ta < tb)
                return -1;
            if (tb < ta)
                return 1;
            break;
        }
        case 2:
        default:
            break;
    }

    if (nameOf (a) < nameOf (b))
        return -1;
    if (nameOf (b) < nameOf (a))
        return 1;
    if (a.absolutePath.string() < b.absolutePath.string())
        return -1;
    if (b.absolutePath.string() < a.absolutePath.string())
        return 1;
    if (a.bankSlot < b.bankSlot)
        return -1;
    if (b.bankSlot < a.bankSlot)
        return 1;
    return 0;
}

void PatchBrowser::applyColumnSort (std::vector<PatchEntry>& voices, bool keepBankGroups) const
{
    const bool fwd = sortForwards;
    auto less = [this, fwd] (const PatchEntry& a, const PatchEntry& b)
    {
        const int c = compareEntries (a, b);
        if (c == 0)
            return false;
        return fwd ? (c < 0) : (c > 0);
    };

    if (! keepBankGroups)
    {
        std::stable_sort (voices.begin(), voices.end(), less);
        return;
    }

    // Keep voices from the same bank file together; sort within each bank, then
    // order banks by the sort key of their first voice (except Slot — keep file order).
    std::stable_sort (voices.begin(), voices.end(), [] (const PatchEntry& a, const PatchEntry& b)
    {
        if (a.absolutePath != b.absolutePath)
            return a.absolutePath.string() < b.absolutePath.string();
        return false;
    });

    size_t i = 0;
    while (i < voices.size())
    {
        size_t j = i + 1;
        while (j < voices.size() && voices[j].absolutePath == voices[i].absolutePath)
            ++j;
        std::stable_sort (voices.begin() + static_cast<std::ptrdiff_t> (i),
                          voices.begin() + static_cast<std::ptrdiff_t> (j),
                          less);
        i = j;
    }

    if (sortColumnId == 5)
        return;

    std::vector<std::vector<PatchEntry>> groups;
    i = 0;
    while (i < voices.size())
    {
        size_t j = i + 1;
        while (j < voices.size() && voices[j].absolutePath == voices[i].absolutePath)
            ++j;
        groups.emplace_back (voices.begin() + static_cast<std::ptrdiff_t> (i),
                             voices.begin() + static_cast<std::ptrdiff_t> (j));
        i = j;
    }

    std::stable_sort (groups.begin(), groups.end(), [&] (const std::vector<PatchEntry>& ga, const std::vector<PatchEntry>& gb)
    {
        return less (ga.front(), gb.front());
    });

    voices.clear();
    for (auto& g : groups)
        voices.insert (voices.end(), std::make_move_iterator (g.begin()), std::make_move_iterator (g.end()));
}

void PatchBrowser::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    sortColumnId = newSortColumnId;
    sortForwards = isForwards;
    rebuildFiltered();
}

void PatchBrowser::rebuildFiltered()
{
    FavoritesStore empty;
    const auto* store = favStore != nullptr ? favStore : &empty;

    std::vector<PatchEntry> scoped;
    scoped.reserve (all.size());
    for (const auto& e : all)
    {
        if (bankFileView)
        {
            if (isBankFileVoice (e))
                scoped.push_back (e);
        }
        else
        {
            if (! isBankFileVoice (e))
                scoped.push_back (e);
        }
    }

    std::unordered_map<uint64_t, int> counts;
    for (const auto& e : scoped)
        ++counts[e.contentId];
    int dupeVoices = 0;
    for (const auto& e : scoped)
        if (counts[e.contentId] >= 2)
            ++dupeVoices;

    auto q = LibraryFilter::parse (search.getText().toStdString(), favOnly.getToggleState());
    const auto recentIds = recentStore != nullptr ? recentStore->contentIds() : std::unordered_set<uint64_t> {};
    const auto* recentPtr = (q.recentOnly && recentStore != nullptr) ? &recentIds : nullptr;
    auto voices = LibraryFilter::apply (std::move (scoped), q, *store, tagStore, recentPtr);

    const bool grouping = bankFileView && groupByBank && q.orGroups.empty() && ! q.duplicatesOnly;
    applyColumnSort (voices, grouping);
    if (hideDuplicates && ! q.duplicatesOnly)
        voices = LibraryFilter::keepFirstByContentId (std::move (voices));

    rows = BrowserList::buildRows (std::move (voices), grouping);
    lastSentRow = -1;

    int shown = 0;
    for (const auto& r : rows)
        if (r.kind == BrowserRowKind::voice)
            ++shown;

    lastStats = { static_cast<int> (scoped.size()), shown, dupeVoices };
    if (onStatsChanged)
        onStatsChanged (lastStats);

    table.updateContent();
    table.repaint();
    updateStickyHeader();
    resized();
}

void PatchBrowser::updateStickyHeader()
{
    if (! (bankFileView && groupByBank))
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
        stickyBank.setText ("Bank: " + toJuce (rows[static_cast<size_t> (row)].entry.fileName),
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

juce::String PatchBrowser::folderColumnText (const PatchEntry& e)
{
    if (! e.relativePath.empty())
    {
        const auto parent = std::filesystem::path (e.relativePath).parent_path();
        if (! parent.empty())
            return juce::String::fromUTF8 (parent.generic_string().c_str());
    }
    if (! e.baseFolder.empty())
        return juce::String::fromUTF8 (e.baseFolder.filename().string().c_str());
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

    const auto& e = r.entry;
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
            if (tagStore != nullptr)
            {
                const auto tags = tagStore->getTags (e.contentId);
                juce::StringArray parts;
                for (const auto& t : tags)
                    parts.add (juce::String::fromUTF8 (t.c_str()));
                text = parts.joinIntoString (", ");
            }
            break;
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
        return "Click Tags cell to edit (OK / Cancel / Clear)";
    const auto& e = rows[static_cast<size_t> (rowNumber)].entry;
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
    draggedVoice = rows[static_cast<size_t> (row)].entry;
    return juce::var ("fmlib-voice");
}

void PatchBrowser::loadRow (int row, bool loadBank)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())) || ! onLoad)
        return;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return;
    if (! loadBank && row == lastSentRow)
        return;
    lastSentRow = row;
    onLoad (rows[static_cast<size_t> (row)].entry, loadBank);
    updateStickyHeader();
}

void PatchBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (! juce::isPositiveAndBelow (lastRowSelected, static_cast<int> (rows.size())))
        return;
    if (rows[static_cast<size_t> (lastRowSelected)].kind == BrowserRowKind::sectionHeader)
    {
        const int next = nextSelectableRow (lastRowSelected, 1);
        if (next >= 0 && next != lastRowSelected)
            table.selectRow (next, false, true);
        return;
    }
    loadRow (lastRowSelected, false);
}

void PatchBrowser::editTagsForRow (int row)
{
    if (tagStore == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;
    if (rows[static_cast<size_t> (row)].kind != BrowserRowKind::voice)
        return;

    const auto id = rows[static_cast<size_t> (row)].entry.contentId;
    const auto name = toJuce (rows[static_cast<size_t> (row)].entry.voiceName);
    auto current = tagStore->getTags (id);

    auto* aw = new juce::AlertWindow ("Edit tags",
                                      "Tags for \"" + name + "\" (comma-separated):",
                                      juce::AlertWindow::QuestionIcon);
    themeAlertWindow (*aw, getLookAndFeel());
    juce::StringArray parts;
    for (const auto& t : current)
        parts.add (toJuce (t));
    aw->addTextEditor ("tags", parts.joinIntoString (", "), "Tags");
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

void PatchBrowser::cellClicked (int row, int columnId, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (rows.size())))
        return;

    table.grabKeyboardFocus();
    if (onListFocused)
        onListFocused();

    if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
        return;

    if (columnId == 1)
    {
        const auto& entry = rows[static_cast<size_t> (row)].entry;
        if (onFav)
            onFav (entry.contentId);
        table.repaintRow (row);
        return;
    }

    if (columnId == 6 || e.mods.isPopupMenu())
    {
        editTagsForRow (row);
        return;
    }

    if (row == table.getSelectedRow())
    {
        lastSentRow = -1;
        loadRow (row, false);
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
            loadRow (row + 1, true);
        return;
    }
    loadRow (row, true);
}

} // namespace fmlib
