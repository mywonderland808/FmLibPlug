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

std::vector<std::string> splitKeepExact (const std::string& text, const std::string& delim)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size())
    {
        const auto pos = text.find (delim, start);
        if (pos == std::string::npos)
        {
            parts.push_back (text.substr (start));
            break;
        }
        parts.push_back (text.substr (start, pos - start));
        start = pos + delim.size();
    }
    return parts;
}

std::vector<std::string> splitWords (const std::string& text)
{
    std::vector<std::string> words;
    std::string cur;
    for (char ch : text)
    {
        if (std::isspace (static_cast<unsigned char> (ch)))
        {
            if (! cur.empty())
            {
                words.push_back (cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back (ch);
        }
    }
    if (! cur.empty())
        words.push_back (cur);
    return words;
}

bool isOperatorWord (const std::string& word)
{
    return word == "AND" || word == "OR";
}

LibraryFilterAndGroup parseAndGroup (const std::string& clause)
{
    LibraryFilterAndGroup g;
    for (auto piece : splitKeepExact (clause, " AND "))
    {
        trimInPlace (piece);
        if (piece.empty())
            continue;

        std::string textBuf;
        auto flushText = [&]()
        {
            trimInPlace (textBuf);
            if (textBuf.empty())
                return;
            LibraryFilterAtom a;
            a.kind = LibraryFilterAtom::Kind::text;
            a.value = asciiLower (textBuf);
            g.atoms.push_back (std::move (a));
            textBuf.clear();
        };

        for (const auto& word : splitWords (piece))
        {
            // Uppercase AND/OR are operators only (e.g. stray "OR AND tag:x"); skip as atoms.
            if (isOperatorWord (word))
            {
                flushText();
                continue;
            }

            const auto low = asciiLower (word);
            if (low == "fav:" || low == "star:")
            {
                flushText();
                g.atoms.push_back ({ LibraryFilterAtom::Kind::favorites, {} });
            }
            else if (low == "dupe:" || low == "dup:" || low == ":dupe" || low == ":dup")
            {
                flushText();
                g.atoms.push_back ({ LibraryFilterAtom::Kind::duplicates, {} });
            }
            else if (low == "recent:")
            {
                flushText();
                g.atoms.push_back ({ LibraryFilterAtom::Kind::recent, {} });
            }
            else if (low == ":singles" || low == "singles:" || low == ":single" || low == "single:")
            {
                flushText();
                g.atoms.push_back ({ LibraryFilterAtom::Kind::singles, {} });
            }
            else if (low.rfind ("tag:", 0) == 0)
            {
                flushText();
                LibraryFilterAtom a;
                a.kind = LibraryFilterAtom::Kind::tag;
                a.value = low.substr (4);
                trimInPlace (a.value);
                g.atoms.push_back (std::move (a));
            }
            else
            {
                if (! textBuf.empty())
                    textBuf.push_back (' ');
                textBuf += word;
            }
        }
        flushText();
    }
    return g;
}

bool atomMatches (const LibraryFilterAtom& atom,
                  const PatchEntry& e,
                  const FavoritesStore& favorites,
                  const TagStore* tags,
                  const std::unordered_set<uint64_t>* dupeIds,
                  const std::unordered_set<uint64_t>* recentIds)
{
    switch (atom.kind)
    {
        case LibraryFilterAtom::Kind::favorites:
            return favorites.isFavorite (e.contentId);
        case LibraryFilterAtom::Kind::duplicates:
            return dupeIds != nullptr && dupeIds->count (e.contentId) > 0;
        case LibraryFilterAtom::Kind::recent:
            return recentIds != nullptr && recentIds->count (e.contentId) > 0;
        case LibraryFilterAtom::Kind::singles:
            return isSingleVoiceFile (e);
        case LibraryFilterAtom::Kind::tag:
            if (atom.value.empty())
                return true;
            return tags != nullptr && tags->hasTag (e.contentId, atom.value);
        case LibraryFilterAtom::Kind::text:
        default:
            break;
    }

    if (atom.value.empty())
        return true;
    const auto& vn = e.voiceNameLower.empty() ? asciiLower (e.voiceName) : e.voiceNameLower;
    const auto& fn = e.fileNameLower.empty() ? asciiLower (e.fileName) : e.fileNameLower;
    const auto& rp = e.relativePathLower.empty() ? asciiLower (e.relativePath) : e.relativePathLower;
    return contains (vn, atom.value) || contains (fn, atom.value) || contains (rp, atom.value);
}

bool groupMatches (const LibraryFilterAndGroup& g,
                   const PatchEntry& e,
                   const FavoritesStore& favorites,
                   const TagStore* tags,
                   const std::unordered_set<uint64_t>* dupeIds,
                   const std::unordered_set<uint64_t>* recentIds)
{
    if (g.atoms.empty())
        return true;
    for (const auto& a : g.atoms)
        if (! atomMatches (a, e, favorites, tags, dupeIds, recentIds))
            return false;
    return true;
}
} // namespace

LibraryFilterQuery LibraryFilter::parse (const std::string& raw, bool favoritesToggle)
{
    LibraryFilterQuery q;
    q.favoritesOnly = favoritesToggle;
    auto text = raw;
    trimInPlace (text);

    if (! text.empty())
    {
        for (auto clause : splitKeepExact (text, " OR "))
        {
            trimInPlace (clause);
            if (clause.empty() || isOperatorWord (clause))
                continue;
            auto group = parseAndGroup (clause);
            if (! group.atoms.empty())
                q.orGroups.push_back (std::move (group));
        }
    }

    for (const auto& g : q.orGroups)
        for (const auto& a : g.atoms)
        {
            if (a.kind == LibraryFilterAtom::Kind::duplicates)
                q.duplicatesOnly = true;
            if (a.kind == LibraryFilterAtom::Kind::recent)
                q.recentOnly = true;
        }

    return q;
}

std::vector<PatchEntry> LibraryFilter::apply (std::vector<PatchEntry> all,
                                              const LibraryFilterQuery& query,
                                              const FavoritesStore& favorites,
                                              const TagStore* tags,
                                              const std::unordered_set<uint64_t>* recentIds)
{
    if (query.orGroups.empty() && ! query.favoritesOnly && ! query.duplicatesOnly)
        return all;

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
    const std::unordered_set<uint64_t>* dupePtr = query.duplicatesOnly ? &dupeIds : nullptr;

    std::vector<PatchEntry> out;
    out.reserve (all.size());
    for (auto& e : all)
    {
        // Favorites toggle is a global AND (UI checkbox).
        if (query.favoritesOnly && ! favorites.isFavorite (e.contentId))
            continue;

        if (! query.orGroups.empty())
        {
            bool any = false;
            for (const auto& g : query.orGroups)
            {
                if (groupMatches (g, e, favorites, tags, dupePtr, recentIds))
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

std::string LibraryFilter::toggleAndTagToken (const std::string& query, const std::string& tagName,
                                              TagChipCombine combine)
{
    auto tag = asciiLower (tagName);
    trimInPlace (tag);
    if (tag.empty())
        return query;

    const std::string token = "tag:" + tag;
    auto text = query;
    trimInPlace (text);
    if (text.empty())
        return token;

    // Strip trailing AND/OR so chip combines always see a clean expression,
    // but remember them for plain (replace) clicks that should append.
    enum class TrailingOp { none, andOp, orOp };
    TrailingOp trailing = TrailingOp::none;
    auto stripTrailingOps = [&] (std::string& t)
    {
        for (;;)
        {
            trimInPlace (t);
            if (t.size() >= 4 && t.compare (t.size() - 4, 4, " AND") == 0)
            {
                t.resize (t.size() - 4);
                trailing = TrailingOp::andOp;
                continue;
            }
            if (t == "AND")
            {
                t.clear();
                trailing = TrailingOp::andOp;
                continue;
            }
            if (t.size() >= 3 && t.compare (t.size() - 3, 3, " OR") == 0)
            {
                t.resize (t.size() - 3);
                trailing = TrailingOp::orOp;
                continue;
            }
            if (t == "OR")
            {
                t.clear();
                trailing = TrailingOp::orOp;
                continue;
            }
            break;
        }
        trimInPlace (t);
    };
    stripTrailingOps (text);
    if (text.empty())
        return token;

    const auto parsed = parse (text, false);

    auto appendAtom = [] (std::string& body, const LibraryFilterAtom& a)
    {
        switch (a.kind)
        {
            case LibraryFilterAtom::Kind::tag: body += "tag:" + a.value; break;
            case LibraryFilterAtom::Kind::favorites: body += "fav:"; break;
            case LibraryFilterAtom::Kind::duplicates: body += "dupe:"; break;
            case LibraryFilterAtom::Kind::recent: body += "recent:"; break;
            case LibraryFilterAtom::Kind::singles: body += ":singles"; break;
            case LibraryFilterAtom::Kind::text:
            default: body += a.value; break;
        }
    };

    auto serializeGroup = [&] (const LibraryFilterAndGroup& g, bool ensureTag) -> std::string
    {
        std::string body;
        bool hasTag = false;
        for (size_t i = 0; i < g.atoms.size(); ++i)
        {
            if (g.atoms[i].kind == LibraryFilterAtom::Kind::tag && g.atoms[i].value == tag)
                hasTag = true;
            if (i > 0)
                body += " AND ";
            appendAtom (body, g.atoms[i]);
        }
        if (ensureTag && ! hasTag)
        {
            if (! body.empty())
                body += " AND ";
            body += token;
        }
        return body;
    };

    auto groupHasTag = [&] (const LibraryFilterAndGroup& g)
    {
        for (const auto& a : g.atoms)
            if (a.kind == LibraryFilterAtom::Kind::tag && a.value == tag)
                return true;
        return false;
    };

    int groupsWithTag = 0;
    for (const auto& g : parsed.orGroups)
        if (groupHasTag (g))
            ++groupsWithTag;
    const bool presentEverywhere = ! parsed.orGroups.empty()
                                && groupsWithTag == static_cast<int> (parsed.orGroups.size());
    const bool presentAnywhere = groupsWithTag > 0;

    // Shift+AND with multiple OR groups: expand to DNF, or remove if every branch already has it.
    if (combine == TagChipCombine::withAnd && parsed.orGroups.size() > 1)
    {
        std::string body;
        if (presentEverywhere)
        {
            // Toggle off: drop the tag from every branch.
            for (const auto& g : parsed.orGroups)
            {
                std::vector<LibraryFilterAtom> kept;
                for (const auto& a : g.atoms)
                    if (! (a.kind == LibraryFilterAtom::Kind::tag && a.value == tag))
                        kept.push_back (a);
                if (kept.empty())
                    continue;
                if (! body.empty())
                    body += " OR ";
                for (size_t i = 0; i < kept.size(); ++i)
                {
                    if (i > 0)
                        body += " AND ";
                    appendAtom (body, kept[i]);
                }
            }
            return body;
        }

        // Ensure every OR branch includes the tag.
        for (const auto& g : parsed.orGroups)
        {
            if (g.atoms.empty())
                continue;
            if (! body.empty())
                body += " OR ";
            body += serializeGroup (g, true);
        }
        return body.empty() ? token : body;
    }

    if (! presentAnywhere)
    {
        if (combine == TagChipCombine::withOr)
            return text + " OR " + token;
        if (combine == TagChipCombine::withAnd)
            return text + " AND " + token;
        // Replace: honour a trailing operator the user typed before clicking.
        if (trailing == TrailingOp::andOp)
            return text + " AND " + token;
        if (trailing == TrailingOp::orOp)
            return text + " OR " + token;
        return token;
    }

    // Present somewhere: remove that tag atom from all groups (rebuild).
    std::string body;
    for (const auto& g : parsed.orGroups)
    {
        std::vector<LibraryFilterAtom> kept;
        for (const auto& a : g.atoms)
            if (! (a.kind == LibraryFilterAtom::Kind::tag && a.value == tag))
                kept.push_back (a);
        if (kept.empty())
            continue;
        if (! body.empty())
            body += " OR ";
        for (size_t i = 0; i < kept.size(); ++i)
        {
            if (i > 0)
                body += " AND ";
            appendAtom (body, kept[i]);
        }
    }

    return body;
}

} // namespace fmlib
