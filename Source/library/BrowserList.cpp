#include "library/BrowserList.h"
#include <algorithm>

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

std::vector<PatchEntry> BrowserList::sortFlatByName (std::vector<PatchEntry> entries)
{
    std::sort (entries.begin(), entries.end(), [] (const PatchEntry& a, const PatchEntry& b)
    {
        if (a.voiceName != b.voiceName)
            return a.voiceName < b.voiceName;
        return a.absolutePath.string() < b.absolutePath.string();
    });
    return entries;
}

std::vector<BrowserRow> BrowserList::buildRows (const std::vector<PatchEntry>& voices, bool groupByBank)
{
    std::vector<BrowserRow> rows;
    if (! groupByBank)
    {
        for (const auto& e : voices)
        {
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.entry = e;
            rows.push_back (std::move (r));
        }
        return rows;
    }

    size_t i = 0;
    while (i < voices.size())
    {
        const auto& path = voices[i].absolutePath;
        size_t j = i + 1;
        while (j < voices.size() && voices[j].absolutePath == path)
            ++j;

        BrowserRow header;
        header.kind = BrowserRowKind::sectionHeader;
        header.sectionLabel = path.stem().string();
        if (header.sectionLabel.empty())
            header.sectionLabel = voices[i].fileName;
        header.sectionVoiceCount = static_cast<int> (j - i);
        header.entry = voices[i];
        rows.push_back (std::move (header));

        for (size_t k = i; k < j; ++k)
        {
            BrowserRow r;
            r.kind = BrowserRowKind::voice;
            r.entry = voices[k];
            rows.push_back (std::move (r));
        }
        i = j;
    }
    return rows;
}

namespace
{
int findBankStart (const std::vector<BrowserRow>& rows, int fromVoiceRow)
{
    if (fromVoiceRow < 0 || fromVoiceRow >= static_cast<int> (rows.size()))
        return -1;
    // Walk back to first voice of this bank (after optional header)
    int i = fromVoiceRow;
    while (i > 0 && rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::voice
           && rows[static_cast<size_t> (i)].entry.absolutePath
                  == rows[static_cast<size_t> (i - 1)].entry.absolutePath)
        --i;
    if (i > 0 && rows[static_cast<size_t> (i - 1)].kind == BrowserRowKind::sectionHeader
        && rows[static_cast<size_t> (i - 1)].entry.absolutePath == rows[static_cast<size_t> (i)].entry.absolutePath)
        return i; // first voice after header
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

    const auto& path = rows[static_cast<size_t> (curStart)].entry.absolutePath;

    for (int i = curStart - 1; i >= 0; --i)
    {
        if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::sectionHeader)
        {
            // Skip the header for the current bank
            if (i + 1 == curStart)
                continue;
            if (i + 1 < static_cast<int> (rows.size()) && rows[static_cast<size_t> (i + 1)].kind == BrowserRowKind::voice)
                return i + 1;
        }
        else if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
                 && rows[static_cast<size_t> (i)].entry.absolutePath != path)
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

    const auto& path = rows[static_cast<size_t> (curStart)].entry.absolutePath;
    for (int i = curStart + 1; i < static_cast<int> (rows.size()); ++i)
    {
        if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::sectionHeader)
        {
            if (i + 1 < static_cast<int> (rows.size()) && rows[static_cast<size_t> (i + 1)].kind == BrowserRowKind::voice)
                return i + 1;
        }
        else if (rows[static_cast<size_t> (i)].kind == BrowserRowKind::voice
                 && rows[static_cast<size_t> (i)].entry.absolutePath != path)
        {
            return findBankStart (rows, i);
        }
    }
    return std::nullopt;
}

} // namespace fmlib
