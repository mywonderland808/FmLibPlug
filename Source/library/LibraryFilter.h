#pragma once

#include "library/FavoritesStore.h"
#include "library/PatchEntry.h"
#include "library/TagStore.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace fmlib
{

/** One atomic condition: either a tag: match or a free-text substring. */
struct LibraryFilterAtom
{
    enum class Kind { text, tag };
    Kind kind = Kind::text;
    std::string value; // lowercased
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
 *   bass OR piano AND soft   (AND binds tighter than OR within a simple split:
 *     OR-separated clauses; within each clause, AND-separated atoms)
 */
struct LibraryFilterQuery
{
    bool favoritesOnly = false;
    bool duplicatesOnly = false;
    bool recentOnly = false;

    /** OR of AND-groups. Empty means no text/tag constraint. */
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
};

} // namespace fmlib
