#pragma once

#include "library/PatchEntry.h"
#include <unordered_map>
#include <vector>

namespace fmlib
{

class DuplicateDetector
{
public:
    /** Groups entries that share the same contentId (size >= 2). */
    static std::unordered_map<uint64_t, std::vector<PatchEntry>> groupDuplicates (const std::vector<PatchEntry>& entries);
    static bool isDuplicateId (const std::unordered_map<uint64_t, std::vector<PatchEntry>>& groups, uint64_t id);
};

} // namespace fmlib
