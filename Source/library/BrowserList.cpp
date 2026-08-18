#include "library/BrowserList.h"
#include <algorithm>
#include <unordered_map>

namespace fmlib
{

std::vector<PatchEntry> BrowserList::sortGrouped (std::vector<PatchEntry> entries)
{
    std::sort (entries.begin(), entries.end(), [] (const PatchEntry& a, const PatchEntry& b)
    {
        if (a.absolutePath != b.absolutePath)
            return a.absolutePath < b.absolutePath;
        const int sa = a.bankSlot > 0 ? a.bankSlot : 999;
        const int sb = b.bankSlot > 0 ? b.bankSlot : 999;
        if (sa != sb)
            return sa < sb;
        return a.voiceName < b.voiceName;
    });
    return entries;
}

std::vector<BrowserRow> BrowserList::buildRows (std::vector<PatchEntry> voices, bool groupByBank)
{
    std::vector<BrowserRow> rows;
    if (! groupByBank)
    {
        rows.reserve (voices.size());
        for (auto& e : voices)
        {
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.bankPath = e.absolutePath;
            r.entry = std::move (e);
            rows.push_back (std::move (r));
        }
        return rows;
    }

    size_t i = 0;
    while (i < voices.size())
    {
        const auto path = voices[i].absolutePath;
        size_t j = i + 1;
        while (j < voices.size() && voices[j].absolutePath == path)
            ++j;

        BrowserRow header;
        header.kind = BrowserRowKind::sectionHeader;
        header.bankPath = path;
        header.sectionLabel = path.stem().string();
        if (header.sectionLabel.empty())
            header.sectionLabel = voices[i].fileName;
        header.sectionVoiceCount = static_cast<int> (j - i);
        rows.push_back (std::move (header));

        for (size_t k = i; k < j; ++k)
        {
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.bankPath = path;
            r.entry = std::move (voices[k]);
            rows.push_back (std::move (r));
        }
        i = j;
    }
    return rows;
}

BrowserFilterResult BrowserList::filterForBrowser (const std::vector<PatchEntry>& all,
                                                   BrowserScope scope,
                                                   const LibraryFilterQuery& query,
                                                   const FavoritesStore& favorites,
                                                   const TagStore* tags,
                                                   const std::unordered_set<uint64_t>* recentIds)
{
    auto inScope = [scope] (const PatchEntry& e)
    {
        switch (scope)
        {
            case BrowserScope::bankFiles:
                return isBankFileVoice (e);
            case BrowserScope::allVoices:
                return true;
            case BrowserScope::singleSysex:
                return ! isBankFileVoice (e);
        }
        return true;
    };

    std::unordered_map<uint64_t, int> counts;
    counts.reserve (all.size());
    int total = 0;
    for (const auto& e : all)
    {
        if (! inScope (e))
            continue;
        ++total;
        ++counts[e.contentId];
    }

    int dupeVoices = 0;
    for (const auto& [id, n] : counts)
        if (n >= 2)
            dupeVoices += n;

    BrowserFilterResult out;
    out.stats.totalInScope = total;
    out.stats.duplicates = dupeVoices;

    const bool noExpr = query.orGroups.empty() && ! query.favoritesOnly && ! query.duplicatesOnly;
    out.voices.reserve (static_cast<size_t> (total));
    if (noExpr)
    {
        for (const auto& e : all)
            if (inScope (e))
                out.voices.push_back (e);
    }
    else
    {
        std::vector<PatchEntry> scoped;
        scoped.reserve (static_cast<size_t> (total));
        for (const auto& e : all)
            if (inScope (e))
                scoped.push_back (e);
        out.voices = LibraryFilter::apply (std::move (scoped), query, favorites, tags, recentIds);
    }

    out.stats.shown = static_cast<int> (out.voices.size());
    return out;
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

    const char key = nameGroupKey (rows[static_cast<size_t> (cur)].entry.voiceName);
    for (int i = cur - 1; i >= 0; --i)
    {
        if (rows[static_cast<size_t> (i)].kind != BrowserRowKind::voice)
            continue;
        if (nameGroupKey (rows[static_cast<size_t> (i)].entry.voiceName) == key)
            continue;
        const char target = nameGroupKey (rows[static_cast<size_t> (i)].entry.voiceName);
        int start = i;
        while (start > 0 && rows[static_cast<size_t> (start - 1)].kind == BrowserRowKind::voice
               && nameGroupKey (rows[static_cast<size_t> (start - 1)].entry.voiceName) == target)
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

    const char key = nameGroupKey (rows[static_cast<size_t> (cur)].entry.voiceName);
    for (int i = cur + 1; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows[static_cast<size_t> (i)].kind != BrowserRowKind::voice)
            continue;
        if (nameGroupKey (rows[static_cast<size_t> (i)].entry.voiceName) != key)
            return i;
    }
    return std::nullopt;
}

} // namespace fmlib
