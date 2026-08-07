#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_set>

namespace fmlib
{

/** In-memory ring of recently sent voice contentIds (for `recent:` filter). */
class RecentStore
{
public:
    explicit RecentStore (size_t capacity = 32);

    void push (uint64_t contentId);
    void clear() { ring.clear(); }
    std::unordered_set<uint64_t> contentIds() const;

private:
    size_t capacity;
    std::deque<uint64_t> ring;
};

} // namespace fmlib
