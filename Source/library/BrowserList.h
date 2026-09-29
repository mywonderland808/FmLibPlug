#pragma once

#include "library/FavoritesStore.h"
#include "library/LibraryFilter.h"
#include "library/PatchEntry.h"
#include "library/TagStore.h"
#include <filesystem>
#include <functional>
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
    /** Index metadata for voice rows (no VoiceData); empty for section headers. */
    PatchMeta meta;
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
    /** Indices into the `all` vector passed to filterForBrowser (no PatchMeta copies). */
    std::vector<int> voiceIndices;
};

/** Which voices the patch browser includes. */
enum class BrowserScope
{
    bankFiles,     // bankSlot > 0 only (always grouped by file in the UI)
    allVoices      // every voice, flat
};

/** Optional joined-tags string for column sort (UI supplies TagStore lookup). */
using TagDisplayFn = std::function<const std::string&(const PatchMeta&)>;

/** Pure list helpers for bank-aware browsing (Catch2-tested). */
class BrowserList
{
public:
    static std::vector<int> sortGroupedIndices (const std::vector<PatchMeta>& all,
                                                std::vector<int> indices);

    /** Copies PatchMeta into voice rows (headers store path/label only). */
    static std::vector<BrowserRow> buildRows (const std::vector<PatchMeta>& all,
                                              const std::vector<int>& voiceIndices,
                                              bool groupByBank);

    /**
     * Scope by BrowserScope, count dupes, apply search filter.
     * Returns indices into `all` (no PatchMeta / VoiceData copies).
     * Does not hide duplicates — call keepFirstByContentId after sorting so the kept voice matches sort order.
     */
    static BrowserFilterResult filterForBrowser (const std::vector<PatchMeta>& all,
                                                 BrowserScope scope,
                                                 const LibraryFilterQuery& query,
                                                 const FavoritesStore& favorites,
                                                 const TagStore* tags = nullptr,
                                                 const std::unordered_set<uint64_t>* recentIds = nullptr);

    /**
     * Column-aware compare: negative if a < b, positive if a > b, 0 if equal.
     * sortColumnId matches TableListBox column ids (1=fav, 2=name, 3=file, 4=folder, 5=slot, 6=tags).
     */
    static int compareMetas (const PatchMeta& a, const PatchMeta& b,
                             int sortColumnId, bool sortForwards,
                             bool aFavorite, bool bFavorite,
                             const TagDisplayFn& tagsOf = {});

    /** Stable-sort `indices` in place (optionally keeping same-bank voices contiguous). */
    static void applyColumnSort (const std::vector<PatchMeta>& all,
                                 std::vector<int>& indices,
                                 int sortColumnId,
                                 bool sortForwards,
                                 bool keepBankGroups,
                                 const FavoritesStore* favorites = nullptr,
                                 const TagDisplayFn& tagsOf = {});

    static bool isBankFileVoice (const PatchMeta& e) { return fmlib::isBankFileVoice (e); }
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
