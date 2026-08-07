#include "library/AutoTagger.h"
#include "library/PatchEntry.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("AutoTagger tags from name heuristics", "[library][autotag]")
{
    VoiceData v {};
    {
        auto tags = AutoTagger::tagsForVoice (v, "Fat Bass");
        REQUIRE (std::find (tags.begin(), tags.end(), "bass") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "fat") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "DarkPad");
        REQUIRE (std::find (tags.begin(), tags.end(), "dark") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "pad") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "MellowStr");
        REQUIRE (std::find (tags.begin(), tags.end(), "mellow") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "string") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "BANJO");
        REQUIRE (std::find (tags.begin(), tags.end(), "guitar") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "VoxPad");
        REQUIRE (std::find (tags.begin(), tags.end(), "vocal") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "pad") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "PNO1");
        REQUIRE (std::find (tags.begin(), tags.end(), "keys") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "Ld Brass");
        REQUIRE (std::find (tags.begin(), tags.end(), "lead") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "brass") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "Org Perk");
        REQUIRE (std::find (tags.begin(), tags.end(), "organ") != tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "perc") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "Flute Soft");
        REQUIRE (std::find (tags.begin(), tags.end(), "flute") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "Voice Pad");
        REQUIRE (std::find (tags.begin(), tags.end(), "vocal") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "GrandPno");
        REQUIRE (std::find (tags.begin(), tags.end(), "keys") != tags.end());
    }
    {
        auto tags = AutoTagger::tagsForVoice (v, "XYZ123");
        REQUIRE (tags.empty());
    }
}
