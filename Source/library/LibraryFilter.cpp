#include "library/LibraryFilter.h"
#include "util/StringUtils.h"
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace fmlib
{

namespace
{
void trimInPlace (std::string& text)
{
    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.front())))
        text.erase (text.begin());
    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.back())))
        text.pop_back();
}

bool contains (const std::string& hayLower, const std::string& needleLower)
{
    if (needleLower.empty())
        return true;
    return hayLower.find (needleLower) != std::string::npos;
}

std::vector<std::string> splitKeep (const std::string& text, const std::string& delimLower)
{
    std::vector<std::string> parts;
    const auto low = asciiLower (text);
    size_t start = 0;
    while (start <= text.size())
    {
        const auto pos = low.find (delimLower, start);
        if (pos == std::string::npos)
        {
            parts.push_back (text.substr (start));
            break;
        }
        parts.push_back (text.substr (start, pos - start));
        start = pos + delimLower.size();
    }
    return parts;
}

LibraryFilterAtom parseAtom (std::string raw)
{
    trimInPlace (raw);
    LibraryFilterAtom a;
    const auto low = asciiLower (raw);
    if (low.rfind ("tag:", 0) == 0)
    {
        a.kind = LibraryFilterAtom::Kind::tag;
        a.value = low.substr (4);
        trimInPlace (a.value);
    }
    else
    {
        a.kind = LibraryFilterAtom::Kind::text;
        a.value = low;
    }
    return a;
}

LibraryFilterAndGroup parseAndGroup (const std::string& clause)
{
    LibraryFilterAndGroup g;
    for (auto piece : splitKeep (clause, " and "))
    {
        trimInPlace (piece);
        if (piece.empty())
            continue;
        std::string cur;
        auto flush = [&]
        {
            trimInPlace (cur);
            if (! cur.empty())
            {
                auto atom = parseAtom (cur);
                if (! atom.value.empty() || atom.kind == LibraryFilterAtom::Kind::tag)
                    g.atoms.push_back (std::move (atom));
                cur.clear();
            }
        };
        for (size_t i = 0; i < piece.size(); ++i)
        {
            if (std::isspace (static_cast<unsigned char> (piece[i])))
            {
                const auto lowCur = asciiLower (cur);
                if (lowCur.rfind ("tag:", 0) == 0)
                    flush();
                else
                    cur.push_back (' ');
            }
            else
            {
                cur.push_back (piece[i]);
            }
        }
        flush();
    }
    return g;
}

bool atomMatches (const LibraryFilterAtom& atom, const PatchEntry& e, const TagStore* tags)
{
    if (atom.kind == LibraryFilterAtom::Kind::tag)
    {
        if (atom.value.empty())
            return true;
        return tags != nullptr && tags->hasTag (e.contentId, atom.value);
    }

    if (atom.value.empty())
        return true;
    const auto& vn = e.voiceNameLower.empty() ? asciiLower (e.voiceName) : e.voiceNameLower;
    const auto& fn = e.fileNameLower.empty() ? asciiLower (e.fileName) : e.fileNameLower;
    const auto& rp = e.relativePathLower.empty() ? asciiLower (e.relativePath) : e.relativePathLower;
    return contains (vn, atom.value) || contains (fn, atom.value) || contains (rp, atom.value);
}

bool groupMatches (const LibraryFilterAndGroup& g, const PatchEntry& e, const TagStore* tags)
{
    if (g.atoms.empty())
        return true;
    for (const auto& a : g.atoms)
        if (! atomMatches (a, e, tags))
            return false;
    return true;
}
} // namespace

LibraryFilterQuery LibraryFilter::parse (const std::string& raw, bool favoritesToggle)
{
    LibraryFilterQuery q;
    q.favoritesOnly = favoritesToggle;
    auto text = raw;

    auto stripToken = [&] (const std::string& token, auto onFound)
    {
        const auto pos = asciiLower (text).find (token);
        if (pos != std::string::npos)
        {
            onFound();
            text.erase (pos, token.size());
        }
    };

    stripToken ("fav:", [&] { q.favoritesOnly = true; });
    stripToken ("star:", [&] { q.favoritesOnly = true; });
    stripToken ("dupe:", [&] { q.duplicatesOnly = true; });
    stripToken ("dup:", [&] { q.duplicatesOnly = true; });
    stripToken (":dup", [&] { q.duplicatesOnly = true; });
    stripToken (":dupe", [&] { q.duplicatesOnly = true; });
    stripToken ("recent:", [&] { q.recentOnly = true; });

    trimInPlace (text);

    if (! text.empty())
    {
        for (auto clause : splitKeep (text, " or "))
        {
            trimInPlace (clause);
            if (clause.empty())
                continue;
            auto group = parseAndGroup (clause);
            if (! group.atoms.empty())
                q.orGroups.push_back (std::move (group));
        }
    }

    return q;
}

std::vector<PatchEntry> LibraryFilter::apply (std::vector<PatchEntry> all,
                                              const LibraryFilterQuery& query,
                                              const FavoritesStore& favorites,
                                              const TagStore* tags,
                                              const std::unordered_set<uint64_t>* recentIds)
{
    std::unordered_set<uint64_t> dupeIds;
    if (query.duplicatesOnly)
    {
        std::unordered_map<uint64_t, int> counts;
        counts.reserve (all.size());
        for (const auto& e : all)
            ++counts[e.contentId];
        for (const auto& [id, n] : counts)
            if (n >= 2)
                dupeIds.insert (id);
    }

    std::vector<PatchEntry> out;
    out.reserve (all.size());
    for (auto& e : all)
    {
        if (query.favoritesOnly && ! favorites.isFavorite (e.contentId))
            continue;
        if (query.duplicatesOnly && dupeIds.count (e.contentId) == 0)
            continue;
        if (query.recentOnly)
        {
            if (recentIds == nullptr || recentIds->count (e.contentId) == 0)
                continue;
        }

        if (! query.orGroups.empty())
        {
            bool any = false;
            for (const auto& g : query.orGroups)
            {
                if (groupMatches (g, e, tags))
                {
                    any = true;
                    break;
                }
            }
            if (! any)
                continue;
        }

        out.push_back (std::move (e));
    }
    return out;
}

std::vector<PatchEntry> LibraryFilter::keepFirstByContentId (std::vector<PatchEntry> voices)
{
    std::unordered_set<uint64_t> seen;
    seen.reserve (voices.size());
    std::vector<PatchEntry> out;
    out.reserve (voices.size());
    for (auto& e : voices)
    {
        if (! seen.insert (e.contentId).second)
            continue;
        out.push_back (std::move (e));
    }
    return out;
}

} // namespace fmlib
