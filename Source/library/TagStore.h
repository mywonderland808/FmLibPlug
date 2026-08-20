#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace fmlib
{

class TagStore
{
public:
    void clear();
    void setTags (uint64_t contentId, std::vector<std::string> tags);
    void addTag (uint64_t contentId, const std::string& tag);
    void removeTag (uint64_t contentId, const std::string& tag);
    void clearTags (uint64_t contentId);
    /** Tags for this id (empty vector if none). Valid until the next mutating call. */
    const std::vector<std::string>& getTags (uint64_t contentId) const;
    /** Comma-joined tags for sort/display (empty if none). Valid until the next mutating call. */
    const std::string& displayJoined (uint64_t contentId) const;
    bool hasTag (uint64_t contentId, const std::string& tag) const;
    /** Sorted unique tag names used anywhere in the store. */
    std::vector<std::string> allUniqueTags() const;

    void loadFromXml (const juce::XmlElement& root);
    void saveToXml (juce::XmlElement& root) const;

    void loadFromFile (const juce::File& file);
    void saveToFile (const juce::File& file) const;

private:
    void refreshJoined (uint64_t contentId);
    void eraseEntry (uint64_t contentId);

    std::unordered_map<uint64_t, std::vector<std::string>> tagsById;
    std::unordered_map<uint64_t, std::string> joinedById;
};

} // namespace fmlib
