#include "library/RecentStore.h"
#include <algorithm>

namespace fmlib
{

RecentStore::RecentStore (size_t cap)
    : capacity (cap == 0 ? 1 : cap)
{
}

void RecentStore::push (RecentItem item)
{
    ring.erase (std::remove_if (ring.begin(), ring.end(),
                                [&] (const RecentItem& x) { return x.contentId == item.contentId; }),
                ring.end());
    ring.push_front (std::move (item));
    while (ring.size() > capacity)
        ring.pop_back();
}

std::unordered_set<uint64_t> RecentStore::contentIds() const
{
    std::unordered_set<uint64_t> ids;
    ids.reserve (ring.size());
    for (const auto& it : ring)
        ids.insert (it.contentId);
    return ids;
}

void RecentStore::loadFromXml (const juce::XmlElement& root)
{
    ring.clear();
    if (auto* el = root.getChildByName ("recent"))
    {
        for (auto* c = el->getFirstChildElement(); c != nullptr; c = c->getNextElement())
        {
            if (! c->hasTagName ("item"))
                continue;
            RecentItem it;
            it.contentId = static_cast<uint64_t> (c->getStringAttribute ("id").getLargeIntValue());
            it.label = c->getStringAttribute ("label").toStdString();
            it.path = c->getStringAttribute ("path").toStdString();
            ring.push_back (std::move (it));
        }
    }
}

void RecentStore::saveToXml (juce::XmlElement& root) const
{
    auto* el = root.createNewChildElement ("recent");
    for (const auto& it : ring)
    {
        auto* c = el->createNewChildElement ("item");
        c->setAttribute ("id", juce::String (static_cast<juce::int64> (it.contentId)));
        c->setAttribute ("label", juce::String::fromUTF8 (it.label.c_str()));
        c->setAttribute ("path", juce::String::fromUTF8 (it.path.c_str()));
    }
}

} // namespace fmlib
