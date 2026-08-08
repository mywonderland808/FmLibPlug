#pragma once

#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include "library/TagStore.h"
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace fmlib
{

enum class BrowserRowKind
{
    sectionHeader,
    voice
};

struct BrowserRow
{
    BrowserRowKind kind = BrowserRowKind::voice;
    /** Full voice data for voice rows; empty for section headers. */
    PatchEntry entry;
    /** Bank file path for navigation (set on headers and voice rows). */
    std::filesystem::path bankPath;
    std::string sectionLabel;
    int sectionVoiceCount = 0;
};

/** Status-bar counters after Bank/Single scope + search filters. */
struct BrowserStats
{
    int totalInScope = 0; // after Bank/Single file filter
    int shown = 0;        // voices after search / hide-duplicates
    int duplicates = 0;   // voices in scope that share a contentId
};

struct BrowserFilterResult
{
    BrowserStats stats;
    std::vector<PatchEntry> voices;
};

/** Pure list helpers for bank-aware browsing (Catch2-tested). */
class BrowserList
{
public:
    static std::vector<PatchEntry> sortGrouped (std::vector<PatchEntry> entries);
    /** Moves voices into row entries (headers store path/label only). */
    static std::vector<BrowserRow> buildRows (std::vector<PatchEntry> voices, bool groupByBank);

    /**
     * Scope by bank/single view, count dupes, apply search filter.
     * Captures totalInScope before moving scoped entries into apply (regression: must not read size after move).
     * Does not hide duplicates — call keepFirstByContentId after sorting so the kept voice matches sort order.
     */
    static BrowserFilterResult filterForBrowser (const std::vector<PatchEntry>& all,
                                                 bool bankFileView,
                                                 const LibraryFilterQuery& query,
                                                 const FavoritesStore& favorites,
                                                 const TagStore* tags = nullptr,
                                                 const std::unordered_set<uint64_t>* recentIds = nullptr);

    static bool isBankFileVoice (const PatchEntry& e) { return e.bankSlot > 0; }
    static int countVoiceRows (const std::vector<BrowserRow>& rows);

    /** Index into `rows` of the first voice of the previous/next bank. nullopt at ends. */
    static std::optional<int> prevBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
    static std::optional<int> nextBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
};

} // namespace fmlib
