#pragma once

#include "library/FavoritesStore.h"
#include "library/PatchEntry.h"
#include "library/TagStore.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace fmlib
{

/** One atomic condition in a filter expression. */
struct LibraryFilterAtom
{
    enum class Kind { text, tag, favorites, duplicates, recent };
    Kind kind = Kind::text;
    std::string value; // lowercased (text/tag only)
};

/** AND-group of atoms (all must match). */
struct LibraryFilterAndGroup
{
    std::vector<LibraryFilterAtom> atoms;
};

/**
 * Search query.
 * Syntax examples:
 *   piano
 *   tag:bass
 *   tag:bass AND dark
 *   tag:pad OR tag:string
 *   dupe: OR fav:
 * Operators are uppercase only: AND / OR (lowercase "and"/"or" are normal search text).
 * Trailing AND/OR is ignored so the preceding clause still filters.
 */
struct LibraryFilterQuery
{
    /** From the Favorites UI toggle (always AND-combined with the expression). */
    bool favoritesOnly = false;
    /** True if the expression references dupe: (for browser grouping/UI). */
    bool duplicatesOnly = false;
    /** True if the expression references recent: (for recent-id lookup). */
    bool recentOnly = false;

    /** OR of AND-groups. Empty means no expression constraint. */
    std::vector<LibraryFilterAndGroup> orGroups;
};

class LibraryFilter
{
public:
    static LibraryFilterQuery parse (const std::string& raw, bool favoritesToggle);

    /** Moves matching entries out of `all` into the result. */
    static std::vector<PatchEntry> apply (std::vector<PatchEntry> all,
                                          const LibraryFilterQuery& query,
                                          const FavoritesStore& favorites,
                                          const TagStore* tags = nullptr,
                                          const std::unordered_set<uint64_t>* recentIds = nullptr);

    /** Keep first occurrence of each contentId (call after sorting so the kept copy matches sort order). */
    static std::vector<PatchEntry> keepFirstByContentId (std::vector<PatchEntry> voices);

    /** How a missing tag chip combines with the current search query. */
    enum class TagChipCombine
    {
        replace = 0, /**< Replace query with `tag:<name>` (unless it already ends with AND/OR). */
        withAnd = 1, /**< Append `AND tag:<name>`. */
        withOr = 2,  /**< Append `OR tag:<name>`. */
    };

    /**
     * Toggle `tag:<name>` (case-insensitive).
     * Absent + withAnd/withOr: append with that operator (or bare token if query empty /
     * already ends with AND/OR). Absent + replace: if the query ends with AND/OR, append
     * the tag; otherwise replace with only `tag:<name>`.
     * Present: remove that tag atom (rebuilds remaining clauses).
     */
    static std::string toggleAndTagToken (const std::string& query, const std::string& tagName,
                                          TagChipCombine combine = TagChipCombine::replace);
};

} // namespace fmlib
