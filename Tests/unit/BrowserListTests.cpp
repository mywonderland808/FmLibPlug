#include "TestHelpers.h"
#include "library/BrowserList.h"
#include "library/PatchEntry.h"

using namespace fmlib;

static PatchEntry voiceAt (const std::string& path, int slot, const std::string& name)
{
    PatchEntry e;
    e.absolutePath = path;
    e.fileName = std::filesystem::path (path).filename().string();
    e.bankSlot = slot;
    e.voiceName = name;
    e.contentId = static_cast<uint64_t> (slot) + path.size() * 1000;
    return e;
}

TEST_CASE ("BrowserList sortGrouped orders by path then slot", "[library][browser]")
{
    std::vector<PatchEntry> in {
        voiceAt ("/b/bank.syx", 2, "B2"),
        voiceAt ("/a/bank.syx", 2, "A2"),
        voiceAt ("/a/bank.syx", 1, "A1"),
        voiceAt ("/b/bank.syx", 1, "B1"),
    };
    const auto out = BrowserList::sortGrouped (in);
    REQUIRE (out[0].voiceName == "A1");
    REQUIRE (out[1].voiceName == "A2");
    REQUIRE (out[2].voiceName == "B1");
    REQUIRE (out[3].voiceName == "B2");
}

TEST_CASE ("BrowserList buildRows inserts section headers", "[library][browser]")
{
    auto voices = BrowserList::sortGrouped ({
        voiceAt ("/a/a.syx", 1, "A1"),
        voiceAt ("/a/a.syx", 2, "A2"),
        voiceAt ("/b/b.syx", 1, "B1"),
    });
    const auto rows = BrowserList::buildRows (voices, true);
    REQUIRE (rows.size() == 5);
    REQUIRE (rows[0].kind == BrowserRowKind::sectionHeader);
    REQUIRE (rows[1].kind == BrowserRowKind::voice);
    REQUIRE (rows[3].kind == BrowserRowKind::sectionHeader);
}

TEST_CASE ("BrowserList prev next bank jumps", "[library][browser]")
{
    auto voices = BrowserList::sortGrouped ({
        voiceAt ("/a/a.syx", 1, "A1"),
        voiceAt ("/a/a.syx", 2, "A2"),
        voiceAt ("/b/b.syx", 1, "B1"),
        voiceAt ("/c/c.syx", 1, "C1"),
    });
    const auto rows = BrowserList::buildRows (voices, true);
    // rows: H A1 A2 H B1 H C1
    REQUIRE_FALSE (BrowserList::prevBankRow (rows, 1).has_value());
    const auto next = BrowserList::nextBankRow (rows, 1);
    REQUIRE (next.has_value());
    REQUIRE (rows[static_cast<size_t> (*next)].entry.voiceName == "B1");
    const auto prev = BrowserList::prevBankRow (rows, *next);
    REQUIRE (prev.has_value());
    REQUIRE (rows[static_cast<size_t> (*prev)].entry.voiceName == "A1");
    REQUIRE_FALSE (BrowserList::nextBankRow (rows, static_cast<int> (rows.size()) - 1).has_value());
}
