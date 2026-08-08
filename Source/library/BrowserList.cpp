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
            return a.absolutePath.string() < b.absolutePath.string();
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
                                                   bool bankFileView,
                                                   const LibraryFilterQuery& query,
                                                   const FavoritesStore& favorites,
                                                   const TagStore* tags,
                                                   const std::unordered_set<uint64_t>* recentIds)
{
    std::vector<PatchEntry> scoped;
    scoped.reserve (all.size());
    for (const auto& e : all)
    {
        if (bankFileView)
        {
            if (isBankFileVoice (e))
                scoped.push_back (e);
        }
        else if (! isBankFileVoice (e))
        {
            scoped.push_back (e);
        }
    }

    std::unordered_map<uint64_t, int> counts;
    counts.reserve (scoped.size());
    for (const auto& e : scoped)
        ++counts[e.contentId];

    int dupeVoices = 0;
    for (const auto& e : scoped)
        if (counts[e.contentId] >= 2)
            ++dupeVoices;

    BrowserFilterResult out;
    out.stats.totalInScope = static_cast<int> (scoped.size());
    out.stats.duplicates = dupeVoices;

    auto voices = LibraryFilter::apply (std::move (scoped), query, favorites, tags, recentIds);
    out.stats.shown = static_cast<int> (voices.size());
    out.voices = std::move (voices);
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

} // namespace fmlib
