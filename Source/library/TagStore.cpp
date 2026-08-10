#include "library/TagStore.h"
#include "util/StringUtils.h"
#include <algorithm>
#include <unordered_set>

namespace fmlib
{
void TagStore::clear()
{
    tagsById.clear();
}

void TagStore::setTags (uint64_t contentId, std::vector<std::string> tags)
{
    for (auto& t : tags)
        t = asciiLower (std::move (t));
    tags.erase (std::remove_if (tags.begin(), tags.end(), [] (const std::string& t) { return t.empty(); }), tags.end());
    if (tags.empty())
        tagsById.erase (contentId);
    else
        tagsById[contentId] = std::move (tags);
}

void TagStore::addTag (uint64_t contentId, const std::string& tag)
{
    const auto t = asciiLower (tag);
    if (t.empty())
        return;
    auto& v = tagsById[contentId];
    if (std::find (v.begin(), v.end(), t) == v.end())
        v.push_back (t);
}

void TagStore::removeTag (uint64_t contentId, const std::string& tag)
{
    const auto t = asciiLower (tag);
    auto it = tagsById.find (contentId);
    if (it == tagsById.end())
        return;
    auto& v = it->second;
    v.erase (std::remove (v.begin(), v.end(), t), v.end());
    if (v.empty())
        tagsById.erase (it);
}

void TagStore::clearTags (uint64_t contentId)
{
    tagsById.erase (contentId);
}

std::vector<std::string> TagStore::getTags (uint64_t contentId) const
{
    auto it = tagsById.find (contentId);
    if (it == tagsById.end())
        return {};
    return it->second;
}

bool TagStore::hasTag (uint64_t contentId, const std::string& tag) const
{
    const auto t = asciiLower (tag);
    auto it = tagsById.find (contentId);
    if (it == tagsById.end())
        return false;
    return std::find (it->second.begin(), it->second.end(), t) != it->second.end();
}

std::vector<std::string> TagStore::allUniqueTags() const
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;
    for (const auto& entry : tagsById)
        for (const auto& t : entry.second)
            if (seen.insert (t).second)
                out.push_back (t);
    std::sort (out.begin(), out.end());
    return out;
}

void TagStore::loadFromXml (const juce::XmlElement& root)
{
    clear();
    if (auto* tagsEl = root.getChildByName ("tags"))
    {
        for (auto* c = tagsEl->getFirstChildElement(); c != nullptr; c = c->getNextElement())
        {
            if (! c->hasTagName ("voice"))
                continue;
            const auto id = static_cast<uint64_t> (c->getStringAttribute ("id").getLargeIntValue());
            std::vector<std::string> tags;
            for (auto* t = c->getFirstChildElement(); t != nullptr; t = t->getNextElement())
                if (t->hasTagName ("t"))
                    tags.push_back (t->getStringAttribute ("v").toStdString());
            setTags (id, std::move (tags));
        }
    }
}

void TagStore::saveToXml (juce::XmlElement& root) const
{
    auto* tagsEl = root.createNewChildElement ("tags");
    for (const auto& [id, tags] : tagsById)
    {
        auto* voice = tagsEl->createNewChildElement ("voice");
        voice->setAttribute ("id", juce::String (static_cast<juce::int64> (id)));
        for (const auto& t : tags)
            voice->createNewChildElement ("t")->setAttribute ("v", juce::String::fromUTF8 (t.c_str()));
    }
}

void TagStore::loadFromFile (const juce::File& file)
{
    clear();
    if (! file.existsAsFile())
        return;
    if (auto xml = juce::XmlDocument::parse (file))
        loadFromXml (*xml);
}

void TagStore::saveToFile (const juce::File& file) const
{
    auto xml = std::make_unique<juce::XmlElement> ("FmLibPlugTags");
    saveToXml (*xml);
    file.getParentDirectory().createDirectory();
    xml->writeTo (file);
}

} // namespace fmlib
