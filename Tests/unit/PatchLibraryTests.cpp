#include "library/PatchLibrary.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

static PatchEntry makeBankVoice (const std::filesystem::path& path, int slot, uint8_t marker)
{
    PatchEntry e;
    e.absolutePath = path;
    e.fileName = path.filename().string();
    e.bankSlot = slot;
    e.voiceName = "V" + std::to_string (slot);
    e.voice[0] = marker;
    e.contentId = static_cast<uint64_t> (marker) << 8 | static_cast<uint64_t> (slot);
    e.refreshSearchCache();
    return e;
}

TEST_CASE ("BankPathIndex completeBank requires all 32 slots", "[library][bank]")
{
    const std::filesystem::path path = "/tmp/bank.syx";
    std::vector<PatchEntry> entries;
    for (int s = 1; s <= kBankVoiceCount; ++s)
        entries.push_back (makeBankVoice (path, s, static_cast<uint8_t> (s)));

    const auto index = BankPathIndex::build (entries);
    std::array<VoiceData, kBankVoiceCount> bank {};
    REQUIRE (index.completeBank (entries, path, bank));
    REQUIRE (bank[0][0] == 1);
    REQUIRE (bank[31][0] == 32);

    entries.pop_back(); // drop slot 32
    const auto incomplete = BankPathIndex::build (entries);
    REQUIRE_FALSE (incomplete.completeBank (entries, path, bank));
    REQUIRE_FALSE (incomplete.completeBank (entries, "/missing.syx", bank));
}

TEST_CASE ("PatchLibrary getBankVoices uses path index", "[library][bank]")
{
    const std::filesystem::path path = "/lib/a.syx";
    std::vector<PatchEntry> entries;
    for (int s = 1; s <= kBankVoiceCount; ++s)
        entries.push_back (makeBankVoice (path, s, static_cast<uint8_t> (10 + s)));

    PatchLibrary lib;
    lib.replaceEntriesForTest (std::move (entries));

    auto bank = lib.getBankVoices (path);
    REQUIRE (bank.has_value());
    REQUIRE ((*bank)[0][0] == 11);
    REQUIRE ((*bank)[31][0] == 42);
    REQUIRE_FALSE (lib.getBankVoices ("/lib/other.syx").has_value());

    auto entry = lib.findEntry (path, 3);
    REQUIRE (entry.has_value());
    REQUIRE (entry->bankSlot == 3);
    REQUIRE (entry->voice[0] == 13);
}

static PatchEntry makeSingle (const std::filesystem::path& path, uint8_t marker, uint64_t contentId)
{
    PatchEntry e;
    e.absolutePath = path;
    e.fileName = path.filename().string();
    e.bankSlot = -1;
    e.voiceName = "S" + std::to_string (marker);
    e.voice[0] = marker;
    e.contentId = contentId;
    e.refreshSearchCache();
    return e;
}

TEST_CASE ("Concatenated singles resolve by libraryIndex, not first path match", "[library][singles]")
{
    const std::filesystem::path path = "/lib/concat.syx";
    std::vector<PatchEntry> entries;
    entries.push_back (makeSingle (path, 1, 0x1111));
    entries.push_back (makeSingle (path, 2, 0x2222));
    entries.push_back (makeSingle (path, 3, 0x3333));

    PatchLibrary lib;
    lib.replaceEntriesForTest (std::move (entries));

    REQUIRE_FALSE (lib.findEntry (path, -1).has_value());
    REQUIRE_FALSE (lib.findEntry (path, 0).has_value());

    auto a = lib.findEntryByIndex (0);
    auto b = lib.findEntryByIndex (1);
    auto c = lib.findEntryByIndex (2);
    REQUIRE (a.has_value());
    REQUIRE (b.has_value());
    REQUIRE (c.has_value());
    REQUIRE (a->libraryIndex == 0);
    REQUIRE (b->libraryIndex == 1);
    REQUIRE (c->libraryIndex == 2);
    REQUIRE (a->voice[0] == 1);
    REQUIRE (b->voice[0] == 2);
    REQUIRE (c->voice[0] == 3);
    REQUIRE (a->contentId == 0x1111);
    REQUIRE (b->contentId == 0x2222);

    REQUIRE_FALSE (lib.findEntryByIndex (-1).has_value());
    REQUIRE_FALSE (lib.findEntryByIndex (99).has_value());
}
