#pragma once

#include "library/PatchEntry.h"
#include <filesystem>
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
    /** Full voice data for voice rows; empty for section headers. */
    PatchEntry entry;
    /** Bank file path for navigation (set on headers and voice rows). */
    std::filesystem::path bankPath;
    std::string sectionLabel;
    int sectionVoiceCount = 0;
};

/** Pure list helpers for bank-aware browsing (Catch2-tested). */
class BrowserList
{
public:
    static std::vector<PatchEntry> sortGrouped (std::vector<PatchEntry> entries);
    /** Moves voices into row entries (headers store path/label only). */
    static std::vector<BrowserRow> buildRows (std::vector<PatchEntry> voices, bool groupByBank);

    /** Index into `rows` of the first voice of the previous/next bank. nullopt at ends. */
    static std::optional<int> prevBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
    static std::optional<int> nextBankRow (const std::vector<BrowserRow>& rows, int selectedRow);
};

} // namespace fmlib
