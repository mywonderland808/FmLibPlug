#include "library/BrowserList.h"
#include <algorithm>
#include <unordered_map>

namespace fmlib
{

std::vector<int> BrowserList::sortGroupedIndices (const std::vector<PatchMeta>& all,
                                                  std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end(), [&] (int ia, int ib)
    {
        const auto& a = all[static_cast<size_t> (ia)];
        const auto& b = all[static_cast<size_t> (ib)];
        if (a.absolutePath != b.absolutePath)
            return a.absolutePath < b.absolutePath;
        const int sa = a.bankSlot > 0 ? a.bankSlot : 999;
        const int sb = b.bankSlot > 0 ? b.bankSlot : 999;
        if (sa != sb)
            return sa < sb;
        return a.voiceName < b.voiceName;
    });
    return indices;
}

std::vector<BrowserRow> BrowserList::buildRows (const std::vector<PatchMeta>& all,
                                                const std::vector<int>& voiceIndices,
                                                bool groupByBank)
{
    std::vector<BrowserRow> rows;
    if (! groupByBank)
    {
        rows.reserve (voiceIndices.size());
        for (int idx : voiceIndices)
        {
            if (idx < 0 || static_cast<size_t> (idx) >= all.size())
                continue;
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.bankPath = all[static_cast<size_t> (idx)].absolutePath;
            r.meta = all[static_cast<size_t> (idx)];
            rows.push_back (std::move (r));
        }
        return rows;
    }

    size_t i = 0;
    while (i < voiceIndices.size())
    {
        const int firstIdx = voiceIndices[i];
        if (firstIdx < 0 || static_cast<size_t> (firstIdx) >= all.size())
        {
            ++i;
            continue;
        }
        const auto path = all[static_cast<size_t> (firstIdx)].absolutePath;
        size_t j = i + 1;
        while (j < voiceIndices.size())
        {
            const int jIdx = voiceIndices[j];
            if (jIdx < 0 || static_cast<size_t> (jIdx) >= all.size()
                || all[static_cast<size_t> (jIdx)].absolutePath != path)
                break;
            ++j;
        }

        BrowserRow header;
        header.kind = BrowserRowKind::sectionHeader;
        header.bankPath = path;
        header.sectionLabel = path.stem().string();
        if (header.sectionLabel.empty())
            header.sectionLabel = all[static_cast<size_t> (firstIdx)].fileName;
        header.sectionVoiceCount = static_cast<int> (j - i);
        rows.push_back (std::move (header));

        for (size_t k = i; k < j; ++k)
        {
            const int vIdx = voiceIndices[k];
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.bankPath = path;
            r.meta = all[static_cast<size_t> (vIdx)];
            rows.push_back (std::move (r));
        }
        i = j;
    }
    return rows;
}

BrowserFilterResult BrowserList::filterForBrowser (const std::vector<PatchMeta>& all,
                                                   BrowserScope scope,
                                                   const LibraryFilterQuery& query,
                                                   const FavoritesStore& favorites,
                                                   const TagStore* tags,
                                                   const std::unordered_set<uint64_t>* recentIds)
{
    auto inScope = [scope] (const PatchMeta& e)
    {
        return scope != BrowserScope::bankFiles || isBankFileVoice (e);
    };

    std::unordered_map<uint64_t, int> counts;
    counts.reserve (all.size());
    int total = 0;
    std::vector<int> scoped;
    scoped.reserve (all.size());
    for (int i = 0; i < static_cast<int> (all.size()); ++i)
    {
        const auto& e = all[static_cast<size_t> (i)];
        if (! inScope (e))
            continue;
        ++total;
        ++counts[e.contentId];
        scoped.push_back (i);
    }

    int dupeVoices = 0;
    for (const auto& [id, n] : counts)
        if (n >= 2)
            dupeVoices += n;

    BrowserFilterResult out;
    out.stats.totalInScope = total;
    out.stats.duplicates = dupeVoices;

    const bool noExpr = query.orGroups.empty() && ! query.favoritesOnly && ! query.duplicatesOnly;
    if (noExpr)
    {
        out.voiceIndices = std::move (scoped);
    }
    else
    {
        out.voiceIndices = LibraryFilter::matchingIndices (all, query, favorites, tags, recentIds, &scoped);
    }

    out.stats.shown = static_cast<int> (out.voiceIndices.size());
    return out;
}

int BrowserList::compareMetas (const PatchMeta& a, const PatchMeta& b,
                               int sortColumnId, bool sortForwards,
                               bool aFavorite, bool bFavorite,
                               const TagDisplayFn& tagsOf)
{
    auto nameOf = [] (const PatchMeta& e) -> const std::string&
    {
        return e.nameSortKey;
    };
    auto fileOf = [] (const PatchMeta& e) -> const std::string&
    {
        return e.fileNameLower.empty() ? e.fileName : e.fileNameLower;
    };
    auto pathOf = [] (const PatchMeta& e) -> const std::string&
    {
        return e.relativePathLower.empty() ? e.relativePath : e.relativePathLower;
    };

    int primary = 0;
    switch (sortColumnId)
    {
        case 1:
            if (aFavorite != bFavorite)
                primary = aFavorite ? -1 : 1;
            break;
        case 3:
            if (fileOf (a) < fileOf (b))
                primary = -1;
            else if (fileOf (b) < fileOf (a))
                primary = 1;
            break;
        case 4:
            if (pathOf (a) < pathOf (b))
                primary = -1;
            else if (pathOf (b) < pathOf (a))
                primary = 1;
            break;
        case 5:
            if (a.bankSlot < b.bankSlot)
                primary = -1;
            else if (b.bankSlot < a.bankSlot)
                primary = 1;
            break;
        case 6:
        {
            static const std::string empty;
            const auto& ta = tagsOf ? tagsOf (a) : empty;
            const auto& tb = tagsOf ? tagsOf (b) : empty;
            if (ta < tb)
                primary = -1;
            else if (tb < ta)
                primary = 1;
            break;
        }
        case 2:
        default:
            break;
    }

    if (primary != 0)
        return sortForwards ? primary : -primary;

    if (nameOf (a) < nameOf (b))
        return sortForwards ? -1 : 1;
    if (nameOf (b) < nameOf (a))
        return sortForwards ? 1 : -1;
    if (a.absolutePath < b.absolutePath)
        return sortForwards ? -1 : 1;
    if (b.absolutePath < a.absolutePath)
        return sortForwards ? 1 : -1;
    if (a.bankSlot < b.bankSlot)
        return sortForwards ? -1 : 1;
    if (b.bankSlot < a.bankSlot)
        return sortForwards ? 1 : -1;
    return 0;
}

void BrowserList::applyColumnSort (const std::vector<PatchMeta>& all,
                                   std::vector<int>& indices,
                                   int sortColumnId,
                                   bool sortForwards,
                                   bool keepBankGroups,
                                   const FavoritesStore* favorites,
                                   const TagDisplayFn& tagsOf)
{
    if (indices.size() < 2)
        return;

    auto less = [&] (int ia, int ib)
    {
        const auto& a = all[static_cast<size_t> (ia)];
        const auto& b = all[static_cast<size_t> (ib)];
        const bool fa = favorites != nullptr && favorites->isFavorite (a.contentId);
        const bool fb = favorites != nullptr && favorites->isFavorite (b.contentId);
        // compareMetas already applies sortForwards; treat as strict weak ordering.
        return compareMetas (a, b, sortColumnId, sortForwards, fa, fb, tagsOf) < 0;
    };

    if (! keepBankGroups)
    {
        std::stable_sort (indices.begin(), indices.end(), less);
        return;
    }

    // Keep voices from the same bank file together; sort within each bank, then
    // order banks by the sort key of their first voice (except Slot — keep file order).
    std::stable_sort (indices.begin(), indices.end(), [&] (int ia, int ib)
    {
        return all[static_cast<size_t> (ia)].absolutePath < all[static_cast<size_t> (ib)].absolutePath;
    });

    std::vector<std::pair<size_t, size_t>> groups;
    size_t i = 0;
    while (i < indices.size())
    {
        size_t j = i + 1;
        while (j < indices.size()
               && all[static_cast<size_t> (indices[j])].absolutePath
                      == all[static_cast<size_t> (indices[i])].absolutePath)
            ++j;
        std::stable_sort (indices.begin() + static_cast<std::ptrdiff_t> (i),
                          indices.begin() + static_cast<std::ptrdiff_t> (j),
                          less);
        groups.emplace_back (i, j);
        i = j;
    }

    if (sortColumnId != 5)
    {
        std::stable_sort (groups.begin(), groups.end(),
                          [&] (const std::pair<size_t, size_t>& ga, const std::pair<size_t, size_t>& gb)
                          {
                              return less (indices[ga.first], indices[gb.first]);
                          });
        std::vector<int> flattened;
        flattened.reserve (indices.size());
        for (const auto& g : groups)
            flattened.insert (flattened.end(),
                              indices.begin() + static_cast<std::ptrdiff_t> (g.first),
                              indices.begin() + static_cast<std::ptrdiff_t> (g.second));
        indices.swap (flattened);
    }
}

int BrowserList::countVoiceRows (const std::vector<BrowserRow>& rows)
{
    int n = 0;
    for (const auto& r : rows)
        if (r.kind == BrowserRowKind::voice)
            ++n;
    return n;
}

namespace
{
int findBankStart (const std::vector<BrowserRow>& rows, int fromVoiceRow)
{
    if (fromVoiceRow < 0 || fromVoiceRow >= static_cast<int> (rows.size()))
        return -1;
    int i = fromVoiceRow;
    while (i > 0 && rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i)].bankPath
                  == rows[static_cast<size_t> (i - 1)].bankPath)
        --i;
    if (i > 0 && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::sectionHeader
        && rows[static_cast<size_t> (i - 1)].bankPath == rows[static_cast<size_t> (i)].bankPath)
        return i;
    return i;
}

int firstVoiceOfBankContaining (const std::vector<BrowserRow>& rows, int row)
{
    if (row < 0 || row >= static_cast<int> (rows.size()))
        return -1;
    if (rows[static_cast<size_t> (row)].kind == BrowserRowKind::sectionHeader)
    {
        if (row + 1 < static_cast<int> (rows.size()) && rows[static_cast<size_t> (row + 1)].kind == BrowserRowKind::voice)
            return row + 1;
        return -1;
    }
    return findBankStart (rows, row);
}
} // namespace

std::optional<int> BrowserList::prevBankRow (const std::vector<BrowserRow>& rows, int selectedRow)
{
    const int curStart = firstVoiceOfBankContaining (rows, selectedRow);
    if (curStart <= 0)
        return std::nullopt;

    const auto& path = rows[static_cast<size_t> (curStart)].bankPath;

    for (int i = curStart - 1; i >= 0; --i)
    {
        if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::sectionHeader)
        {
            if (i + 1 == curStart)
                continue;
            if (i + 1 < static_cast<int> (rows.size()) && rows[static_cast<size_t> (i + 1)].kind == BrowserRowKind::voice)
                return i + 1;
        }
        else if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
                 && rows[static_cast<size_t> (i)].bankPath != path)
        {
            return findBankStart (rows, i);
        }
    }
    return std::nullopt;
}

std::optional<int> BrowserList::nextBankRow (const std::vector<BrowserRow>& rows, int selectedRow)
{
    const int curStart = firstVoiceOfBankContaining (rows, selectedRow);
    if (curStart < 0)
        return std::nullopt;

    const auto& path = rows[static_cast<size_t> (curStart)].bankPath;
    for (int i = curStart + 1; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::sectionHeader)
        {
            if (i + 1 < static_cast<int> (rows.size()) && rows[static_cast<size_t> (i + 1)].kind == BrowserRowKind::voice)
                return i + 1;
        }
        else if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
                 && rows[static_cast<size_t> (i)].bankPath != path)
        {
            return findBankStart (rows, i);
        }
    }
    return std::nullopt;
}

namespace
{
bool asciiIsAlnum (unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char asciiToLower (unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return static_cast<char> (c - 'A' + 'a');
    return static_cast<char> (c);
}
} // namespace

std::string BrowserList::nameBrowseKey (const std::string& voiceName)
{
    std::string out;
    out.reserve (voiceName.size());
    bool started = false;
    for (const char ch : voiceName)
    {
        const auto c = static_cast<unsigned char> (ch);
        if (! started)
        {
            if (! asciiIsAlnum (c))
                continue;
            started = true;
        }
        out.push_back (asciiToLower (c));
    }
    return out;
}

char BrowserList::nameGroupKey (const std::string& voiceName)
{
    const auto key = nameBrowseKey (voiceName);
    if (key.empty())
        return '#';
    const unsigned char c = static_cast<unsigned char> (key.front());
    if (c >= 'a' && c <= 'z')
        return static_cast<char> (c - 'a' + 'A');
    return '#';
}

std::string BrowserList::nameSortKey (const std::string& voiceName)
{
    if (nameGroupKey (voiceName) != '#')
        return nameBrowseKey (voiceName);

    // Digits, symbols and empty names share one bucket, sorted before A.
    std::string rest;
    rest.reserve (voiceName.size() + 1);
    rest.push_back ('\x01');
    for (const char ch : voiceName)
    {
        const auto c = static_cast<unsigned char> (ch);
        rest.push_back (asciiToLower (c));
    }
    return rest;
}

std::optional<int> BrowserList::prevNameGroupRow (const std::vector<BrowserRow>& rows, int selectedRow)
{
    int cur = selectedRow;
    if (cur < 0 || cur >= static_cast<int> (rows.size()))
        return std::nullopt;
    if (rows[static_cast<size_t> (cur)].kind != BrowserRowKind::voice)
    {
        // Snap to nearest voice at/after selection.
        while (cur < static_cast<int> (rows.size())
               && rows[static_cast<size_t> (cur)].kind != BrowserRowKind::voice)
            ++cur;
        if (cur >= static_cast<int> (rows.size()))
            return std::nullopt;
    }

    const char key = nameGroupKey (rows[static_cast<size_t> (cur)].meta.voiceName);
    for (int i = cur - 1; i >= 0; --i)
    {
        if (rows[static_cast<size_t> (i)].kind != BrowserRowKind::voice)
            continue;
        if (nameGroupKey (rows[static_cast<size_t> (i)].meta.voiceName) == key)
            continue;
        const char target = nameGroupKey (rows[static_cast<size_t> (i)].meta.voiceName);
        int start = i;
        while (start > 0 && rows[static_cast<size_t> (start - 1)].kind == BrowserRowKind::voice
               && nameGroupKey (rows[static_cast<size_t> (start - 1)].meta.voiceName) == target)
            --start;
        return start;
    }
    return std::nullopt;
}

std::optional<int> BrowserList::nextNameGroupRow (const std::vector<BrowserRow>& rows, int selectedRow)
{
    int cur = selectedRow;
    if (cur < 0)
        cur = 0;
    if (cur >= static_cast<int> (rows.size()))
        return std::nullopt;
    if (rows[static_cast<size_t> (cur)].kind != BrowserRowKind::voice)
    {
        while (cur < static_cast<int> (rows.size())
               && rows[static_cast<size_t> (cur)].kind != BrowserRowKind::voice)
            ++cur;
        if (cur >= static_cast<int> (rows.size()))
            return std::nullopt;
    }

    const char key = nameGroupKey (rows[static_cast<size_t> (cur)].meta.voiceName);
    for (int i = cur + 1; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows[static_cast<size_t> (i)].kind != BrowserRowKind::voice)
            continue;
        if (nameGroupKey (rows[static_cast<size_t> (i)].meta.voiceName) != key)
            return i;
    }
    return std::nullopt;
}

} // namespace fmlib
