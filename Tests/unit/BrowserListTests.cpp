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
    const auto rows = BrowserList::buildRows (std::move (voices), true);
    REQUIRE (rows.size() == 5);
    REQUIRE (rows[0].kind == BrowserRowKind::sectionHeader);
    REQUIRE (rows[0].meta.voiceName.empty());
    REQUIRE (rows[0].bankPath == "/a/a.syx");
    REQUIRE (rows[1].kind == BrowserRowKind::voice);
    REQUIRE (rows[3].kind == BrowserRowKind::sectionHeader);
}

TEST_CASE ("BrowserList buildRows flat has no headers", "[library][browser]")
{
    auto voices = BrowserList::sortGrouped ({
        voiceAt ("/a/a.syx", 1, "A1"),
        voiceAt ("/b/b.syx", 1, "B1"),
    });
    const auto rows = BrowserList::buildRows (std::move (voices), false);
    REQUIRE (rows.size() == 2);
    REQUIRE (rows[0].kind == BrowserRowKind::voice);
    REQUIRE (rows[1].kind == BrowserRowKind::voice);
}

TEST_CASE ("BrowserList prev next bank jumps", "[library][browser]")
{
    auto voices = BrowserList::sortGrouped ({
        voiceAt ("/a/a.syx", 1, "A1"),
        voiceAt ("/a/a.syx", 2, "A2"),
        voiceAt ("/b/b.syx", 1, "B1"),
        voiceAt ("/c/c.syx", 1, "C1"),
    });
    const auto rows = BrowserList::buildRows (std::move (voices), true);
    // rows: H A1 A2 H B1 H C1
    REQUIRE_FALSE (BrowserList::prevBankRow (rows, 1).has_value());
    const auto next = BrowserList::nextBankRow (rows, 1);
    REQUIRE (next.has_value());
    REQUIRE (rows[static_cast<size_t> (*next)].meta.voiceName == "B1");
    const auto prev = BrowserList::prevBankRow (rows, *next);
    REQUIRE (prev.has_value());
    REQUIRE (rows[static_cast<size_t> (*prev)].meta.voiceName == "A1");
    REQUIRE_FALSE (BrowserList::nextBankRow (rows, static_cast<int> (rows.size()) - 1).has_value());
}

TEST_CASE ("BrowserList prev next name group jumps", "[library][browser]")
{
    std::vector<BrowserRow> rows;
    auto add = [&] (const std::string& name)
    {
        BrowserRow r;
        r.kind = BrowserRowKind::voice;
        r.meta.voiceName = name;
        rows.push_back (std::move (r));
    };
    add ("Alpha");
    add ("Amber");
    add ("Bravo");
    add ("1-kick");
    add ("2-snare");

    REQUIRE (BrowserList::nameGroupKey ("Alpha") == 'A');
    REQUIRE (BrowserList::nameGroupKey ("1-kick") == '#');
    REQUIRE (BrowserList::nameGroupKey ("2-snare") == '#');
    REQUIRE (BrowserList::nameGroupKey ("***") == '#');
    REQUIRE (BrowserList::nameBrowseKey (" Piano") == "piano");
    REQUIRE (BrowserList::nameBrowseKey ("*EP1") == "ep1");
    REQUIRE (BrowserList::nameBrowseKey ("E.PIANO1") == "e.piano1");
    REQUIRE (BrowserList::nameGroupKey (" piano") == 'P');
    REQUIRE (BrowserList::nameGroupKey ("*Brass") == 'B');
    REQUIRE (BrowserList::nameGroupKey ("PIANO") == 'P');
    REQUIRE (BrowserList::nameGroupKey ("piano") == 'P');

    REQUIRE_FALSE (BrowserList::prevNameGroupRow (rows, 1).has_value());
    const auto nextB = BrowserList::nextNameGroupRow (rows, 1);
    REQUIRE (nextB.has_value());
    REQUIRE (*nextB == 2);
    const auto nextOther = BrowserList::nextNameGroupRow (rows, 2);
    REQUIRE (nextOther.has_value());
    REQUIRE (*nextOther == 3);
    REQUIRE_FALSE (BrowserList::nextNameGroupRow (rows, 3).has_value());
    const auto prevB = BrowserList::prevNameGroupRow (rows, 3);
    REQUIRE (prevB.has_value());
    REQUIRE (*prevB == 2);
    const auto prevA = BrowserList::prevNameGroupRow (rows, 2);
    REQUIRE (prevA.has_value());
    REQUIRE (*prevA == 0);
}

TEST_CASE ("BrowserList name groups ignore leading junk and case", "[library][browser]")
{
    std::vector<BrowserRow> rows;
    auto add = [&] (const std::string& name)
    {
        BrowserRow r;
        r.kind = BrowserRowKind::voice;
        r.meta.voiceName = name;
        rows.push_back (std::move (r));
    };
    // Same order Patch-name sort would produce after nameBrowseKey.
    add ("*Alpha");
    add (" amber");
    add ("Bravo");
    add ("brass");

    REQUIRE (BrowserList::nameGroupKey (rows[0].meta.voiceName) == 'A');
    REQUIRE (BrowserList::nameGroupKey (rows[1].meta.voiceName) == 'A');
    REQUIRE (BrowserList::nameBrowseKey ("*Alpha") < BrowserList::nameBrowseKey (" amber"));

    const auto nextB = BrowserList::nextNameGroupRow (rows, 0);
    REQUIRE (nextB.has_value());
    REQUIRE (*nextB == 2);
    REQUIRE (BrowserList::nextNameGroupRow (rows, 1) == nextB);
    REQUIRE_FALSE (BrowserList::nextNameGroupRow (rows, 2).has_value());
}

TEST_CASE ("BrowserList non-letter names share one A-Z jump group", "[library][browser]")
{
    std::vector<BrowserRow> rows;
    auto add = [&] (const std::string& name)
    {
        BrowserRow r;
        r.kind = BrowserRowKind::voice;
        r.meta.voiceName = name;
        rows.push_back (std::move (r));
    };
    add ("***");
    add ("1-kick");
    add ("2-snare");
    add ("Alpha");
    add ("Bravo");

    REQUIRE (BrowserList::nameGroupKey ("***") == '#');
    REQUIRE (BrowserList::nameGroupKey ("1-kick") == '#');
    REQUIRE (BrowserList::nameGroupKey ("2-snare") == '#');
    REQUIRE (BrowserList::nameSortKey ("1-kick") < BrowserList::nameSortKey ("Alpha"));

    const auto nextA = BrowserList::nextNameGroupRow (rows, 0);
    REQUIRE (nextA.has_value());
    REQUIRE (*nextA == 3);
    REQUIRE (BrowserList::nextNameGroupRow (rows, 1) == nextA);
    REQUIRE (BrowserList::nextNameGroupRow (rows, 2) == nextA);

    const auto prevOther = BrowserList::prevNameGroupRow (rows, 3);
    REQUIRE (prevOther.has_value());
    REQUIRE (*prevOther == 0);
}

TEST_CASE ("BrowserList sortGrouped treats missing slot as last", "[library][browser]")
{
    auto out = BrowserList::sortGrouped ({
        voiceAt ("/a/a.syx", 0, "NoSlot"),
        voiceAt ("/a/a.syx", 1, "Slot1"),
    });
    REQUIRE (out[0].voiceName == "Slot1");
    REQUIRE (out[1].voiceName == "NoSlot");
}
