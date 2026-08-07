#include "library/FavoritesStore.h"

namespace fmlib
{

void FavoritesStore::loadFrom (const juce::StringArray& ids)
{
    favorites.clear();
    for (const auto& s : ids)
        favorites.insert (static_cast<uint64_t> (s.getLargeIntValue()));
}

juce::StringArray FavoritesStore::toStringArray() const
{
    juce::StringArray out;
    for (auto id : favorites)
        out.add (juce::String (static_cast<juce::int64> (id)));
    return out;
}

bool FavoritesStore::isFavorite (uint64_t contentId) const
{
    return favorites.count (contentId) > 0;
}

void FavoritesStore::setFavorite (uint64_t contentId, bool fav)
{
    if (fav)
        favorites.insert (contentId);
    else
        favorites.erase (contentId);
}

void FavoritesStore::toggle (uint64_t contentId)
{
    setFavorite (contentId, ! isFavorite (contentId));
}

} // namespace fmlib
