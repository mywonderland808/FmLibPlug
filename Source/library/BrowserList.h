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

/** Status-bar counters after Bank/List scope + search filters. */
struct BrowserStats
{
    int totalInScope = 0; // after Bank/List file filter
    int shown = 0;        // voices after search / hide-duplicates
    int duplicates = 0;   // voices in scope that share a contentId
};

struct BrowserFilterResult
{
    BrowserStats stats;
    std::vector<PatchEntry> voices;
};

/** Which voices the patch browser includes. */
enum class BrowserScope
{
    bankFiles,     // bankSlot > 0 only (always grouped by file in the UI)
    allVoices      // every voice, flat
};

/** Pure list helpers for bank-aware browsing (Catch2-tested). */
class BrowserList
{
public:
    static std::vector<PatchEntry> sortGrouped (std::vector<PatchEntry> entries);
    /** Moves voices into row entries (headers store path/label only). */
    static std::vector<BrowserRow> buildRows (std::vector<PatchEntry> voices, bool groupByBank);

    /**
     * Scope by BrowserScope, count dupes, apply search filter.
     * Captures totalInScope before moving scoped entries into apply (regression: must not read size after move).
     * Does not hide duplicates — call keepFirstByContentId after sorting so the kept voice matches sort order.
     */
    static BrowserFilterResult filterForBrowser (const std::vector<PatchEntry>& all,
                                                 BrowserScope scope,
                                                 const LibraryFilterQuery& query,
                                                 const FavoritesStore& favorites,
                                                 const TagStore* tags = nullptr,
                                                 const std::unordered_set<uint64_t>* recentIds = nullptr);

    static bool isBankFileVoice (const PatchEntry& e) { return fmlib::isBankFileVoice (e); }
    static int countVoiceRows (const std::vector<BrowserRow>& rows);

    /** Index into `rows` of the first voice of the previous/next bank. nullopt at ends. */
    static std::optional<int> prevBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
    static std::optional<int> nextBankRow (const std::vector<BrowserRow>& rows, int selectedRow);

    /**
     * Lowercase ASCII browse key: skip leading non-alphanumeric (spaces, *, etc.),
     * then keep the rest lowercased. Empty if the name has no A–Z / 0–9.
     * Sort and A–Z jumps must use the same grouping so buckets stay contiguous.
     */
    static std::string nameBrowseKey (const std::string& voiceName);
    /** A–Z from the first letter in nameBrowseKey; digits/punctuation/empty share '#'. */
    static char nameGroupKey (const std::string& voiceName);
    /** Sort key: letter names by nameBrowseKey; the '#' bucket sorts together before A. */
    static std::string nameSortKey (const std::string& voiceName);
    static std::optional<int> prevNameGroupRow (const std::vector<BrowserRow>& rows, int selectedRow);
    static std::optional<int> nextNameGroupRow (const std::vector<BrowserRow>& rows, int selectedRow);
};

} // namespace fmlib
