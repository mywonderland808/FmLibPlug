#include "library/LibraryFilter.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace fmlib
{

namespace
{
std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return s;
}

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
    const auto low = lower (text);
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
    const auto low = lower (raw);
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
        // Also treat whitespace-separated tag: tokens as AND atoms
        std::string cur;
        auto flush = [&] {
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
                // If current token is tag: flush; else keep building multi-word text
                const auto lowCur = lower (cur);
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
    const auto& vn = e.voiceNameLower.empty() ? lower (e.voiceName) : e.voiceNameLower;
    const auto& fn = e.fileNameLower.empty() ? lower (e.fileName) : e.fileNameLower;
    const auto& rp = e.relativePathLower.empty() ? lower (e.relativePath) : e.relativePathLower;
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
        const auto pos = lower (text).find (token);
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

    // Legacy fields for simple callers / older tests
    if (q.orGroups.size() == 1 && q.orGroups[0].atoms.size() == 1)
    {
        const auto& a = q.orGroups[0].atoms[0];
        if (a.kind == LibraryFilterAtom::Kind::tag)
            q.tag = a.value;
        else
            q.text = a.value;
    }
    else if (q.orGroups.size() == 1)
    {
        // Concatenate text atoms for legacy .text (best-effort)
        std::string joined;
        for (const auto& a : q.orGroups[0].atoms)
        {
            if (a.kind == LibraryFilterAtom::Kind::tag && q.tag.empty())
                q.tag = a.value;
            if (a.kind == LibraryFilterAtom::Kind::text)
            {
                if (! joined.empty())
                    joined += " ";
                joined += a.value;
            }
        }
        q.text = joined;
    }

    return q;
}

std::vector<PatchEntry> LibraryFilter::apply (const std::vector<PatchEntry>& all,
                                              const LibraryFilterQuery& query,
                                              const FavoritesStore& favorites,
                                              const TagStore* tags,
                                              const std::unordered_set<uint64_t>* recentIds)
{
    std::unordered_set<uint64_t> dupeIds;
    if (query.duplicatesOnly || query.hideDuplicates)
    {
        std::unordered_map<uint64_t, int> counts;
        counts.reserve (all.size());
        for (const auto& e : all)
            ++counts[e.contentId];
        for (const auto& [id, n] : counts)
            if (n >= 2)
                dupeIds.insert (id);
    }

    // dupe:/dup: must show every copy — ignore hide-duplicates for that query
    const bool hideDupes = query.hideDuplicates && ! query.duplicatesOnly;

    std::unordered_set<uint64_t> seen;
    if (hideDupes)
        seen.reserve (all.size());

    std::vector<PatchEntry> out;
    out.reserve (all.size());
    for (const auto& e : all)
    {
        if (query.favoritesOnly && ! favorites.isFavorite (e.contentId))
            continue;
        if (query.duplicatesOnly && dupeIds.count (e.contentId) == 0)
            continue;
        if (hideDupes)
        {
            if (! seen.insert (e.contentId).second)
                continue;
        }
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
        else
        {
            // Legacy direct field setters (tests / callers that skip parse)
            if (! query.tag.empty())
            {
                if (tags == nullptr || ! tags->hasTag (e.contentId, query.tag))
                    continue;
            }
            if (! query.text.empty())
            {
                const auto needle = lower (query.text);
                const auto& vn = e.voiceNameLower.empty() ? lower (e.voiceName) : e.voiceNameLower;
                const auto& fn = e.fileNameLower.empty() ? lower (e.fileName) : e.fileNameLower;
                const auto& rp = e.relativePathLower.empty() ? lower (e.relativePath) : e.relativePathLower;
                if (! contains (vn, needle) && ! contains (fn, needle) && ! contains (rp, needle))
                    continue;
            }
        }

        out.push_back (e);
    }
    return out;
}

std::vector<PatchEntry> LibraryFilter::keepFirstByContentId (const std::vector<PatchEntry>& voices)
{
    std::unordered_set<uint64_t> seen;
    seen.reserve (voices.size());
    std::vector<PatchEntry> out;
    out.reserve (voices.size());
    for (const auto& e : voices)
    {
        if (! seen.insert (e.contentId).second)
            continue;
        out.push_back (e);
    }
    return out;
}

} // namespace fmlib
