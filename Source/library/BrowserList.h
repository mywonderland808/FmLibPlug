#pragma once

#include "library/PatchEntry.h"
#include <optional>
#include <string>
#include <vector>

namespace fmlib
{

enum class BrowserRowKind
{
    sectionHeader,
    voice
};

struct BrowserRow
{
    BrowserRowKind kind = BrowserRowKind::voice;
    PatchEntry entry;
    std::string sectionLabel;
    int sectionVoiceCount = 0;
};

/** Pure list helpers for bank-aware browsing (Catch2-tested). */
class BrowserList
{
public:
    static std::vector<PatchEntry> sortGrouped (std::vector<PatchEntry> entries);
    static std::vector<PatchEntry> sortFlatByName (std::vector<PatchEntry> entries);
    static std::vector<BrowserRow> buildRows (const std::vector<PatchEntry>& voices, bool groupByBank);

    /** Index into `rows` of the first voice of the previous/next bank. nullopt at ends. */
    static std::optional<int> prevBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
    static std::optional<int> nextBankRow (const std::vector<BrowserRow>& rows, int selectedRow);

    static bool isVoiceRow (const BrowserRow& r) { return r.kind == BrowserRowKind::voice; }
};

} // namespace fmlib
