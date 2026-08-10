#include "TestHelpers.h"
#include "library/DeviceBuffer.h"
#include "library/PatchEntry.h"

using namespace fmlib;

static PatchEntry makeEntry (uint8_t seed)
{
    PatchEntry e;
    e.voice = {};
    e.voice[0] = seed;
    e.contentId = contentIdFromVoice (e.voice);
    e.voiceName = "V";
    return e;
}

TEST_CASE ("DeviceBuffer dirty and asBank", "[library][buffer]")
{
    DeviceBuffer buf;
    REQUIRE (buf.empty());
    REQUIRE_FALSE (buf.isDirty());
    REQUIRE_FALSE (buf.asBank().has_value());

    std::vector<PatchEntry> one { makeEntry (1) };
    buf.setVoices (one);
    REQUIRE (buf.isDirty());
    REQUIRE_FALSE (buf.isFullBank());
    REQUIRE_FALSE (buf.asBank().has_value());

    buf.markSaved();
    REQUIRE_FALSE (buf.isDirty());

    std::vector<PatchEntry> full;
    full.reserve (32);
    for (int i = 0; i < 32; ++i)
        full.push_back (makeEntry (static_cast<uint8_t> (i + 1)));
    buf.setVoices (full);
    REQUIRE (buf.isFullBank());
    REQUIRE (buf.isDirty());
    const auto bank = buf.asBank();
    REQUIRE (bank.has_value());
    REQUIRE ((*bank)[0][0] == 1);

    buf.clear();
    REQUIRE (buf.empty());
    REQUIRE_FALSE (buf.isDirty());
}

TEST_CASE ("DeviceBuffer ensureEditableBank expands partial buffer", "[library][buffer]")
{
    DeviceBuffer buf;
    buf.setVoices ({ makeEntry (9) });
    REQUIRE_FALSE (buf.isFullBank());
    buf.markSaved();
    buf.ensureEditableBank();
    REQUIRE (buf.isFullBank());
    REQUIRE (buf.isDirty());
    REQUIRE (buf.getVoices().front().voice[0] == 9);
    REQUIRE (buf.getVoices()[1].voiceName == "(empty)");
}

TEST_CASE ("DeviceBuffer swapVoices exchanges slots", "[library][buffer]")
{
    DeviceBuffer buf;
    std::vector<PatchEntry> voices;
    for (int i = 0; i < 4; ++i)
    {
        auto e = makeEntry (static_cast<uint8_t> (i + 1));
        e.voiceName = "V" + std::to_string (i + 1);
        e.bankSlot = i + 1;
        voices.push_back (std::move (e));
    }
    buf.setVoices (std::move (voices));

    buf.swapVoices (1, 3); // V2 <-> V4
    REQUIRE (buf.getVoices()[1].voiceName == "V4");
    REQUIRE (buf.getVoices()[3].voiceName == "V2");
    REQUIRE (buf.getVoices()[1].bankSlot == 2);
    REQUIRE (buf.getVoices()[3].bankSlot == 4);
    REQUIRE (buf.getVoices()[1].voice[0] == 4);
    REQUIRE (buf.getVoices()[3].voice[0] == 2);
}
