#include "library/DuplicateDetector.h"

namespace fmlib
{

std::unordered_map<uint64_t, std::vector<PatchEntry>> DuplicateDetector::groupDuplicates (const std::vector<PatchEntry>& entries)
{
    std::unordered_map<uint64_t, std::vector<PatchEntry>> all;
    for (const auto& e : entries)
        all[e.contentId].push_back (e);

    std::unordered_map<uint64_t, std::vector<PatchEntry>> dupes;
    for (auto& [id, group] : all)
        if (group.size() >= 2)
            dupes.emplace (id, std::move (group));
    return dupes;
}

bool DuplicateDetector::isDuplicateId (const std::unordered_map<uint64_t, std::vector<PatchEntry>>& groups, uint64_t id)
{
    return groups.count (id) > 0;
}

} // namespace fmlib
