#include "library/RecentStore.h"
#include <algorithm>

namespace fmlib
{

RecentStore::RecentStore (size_t cap)
    : capacity (cap == 0 ? 1 : cap)
{
}

void RecentStore::push (uint64_t contentId)
{
    ring.erase (std::remove (ring.begin(), ring.end(), contentId), ring.end());
    ring.push_front (contentId);
    while (ring.size() > capacity)
        ring.pop_back();
}

std::unordered_set<uint64_t> RecentStore::contentIds() const
{
    return { ring.begin(), ring.end() };
}

} // namespace fmlib
