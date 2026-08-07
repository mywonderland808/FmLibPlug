#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <juce_core/juce_core.h>
#include <string>
#include <unordered_set>

namespace fmlib
{

struct RecentItem
{
    uint64_t contentId = 0;
    std::string label;
    std::string path;
};

class RecentStore
{
public:
    explicit RecentStore (size_t capacity = 32);

    void push (RecentItem item);
    const std::deque<RecentItem>& items() const { return ring; }
    void clear() { ring.clear(); }
    std::unordered_set<uint64_t> contentIds() const;

    void loadFromXml (const juce::XmlElement& root);
    void saveToXml (juce::XmlElement& root) const;

private:
    size_t capacity;
    std::deque<RecentItem> ring;
};

} // namespace fmlib
