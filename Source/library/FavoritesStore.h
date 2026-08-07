#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include <unordered_set>

namespace fmlib
{

class FavoritesStore
{
public:
    void loadFrom (const juce::StringArray& ids);
    juce::StringArray toStringArray() const;

    bool isFavorite (uint64_t contentId) const;
    void setFavorite (uint64_t contentId, bool fav);
    void toggle (uint64_t contentId);

private:
    std::unordered_set<uint64_t> favorites;
};

} // namespace fmlib
