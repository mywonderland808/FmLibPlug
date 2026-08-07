#include "library/AutoTagger.h"
#include "library/DuplicateDetector.h"
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
        REQUIRE (std::find (tags.begin(), tags.end(), "untagged") == tags.end());
        REQUIRE (std::find (tags.begin(), tags.end(), "algo-low") == tags.end());
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

TEST_CASE ("DuplicateDetector groups same contentId", "[library][dupes]")
{
    PatchEntry a, b, c;
    a.contentId = 1;
    b.contentId = 1;
    c.contentId = 2;
    a.voiceName = "A";
    b.voiceName = "B";
    c.voiceName = "C";
    const auto groups = DuplicateDetector::groupDuplicates ({ a, b, c });
    REQUIRE (groups.size() == 1);
    REQUIRE (DuplicateDetector::isDuplicateId (groups, 1));
    REQUIRE_FALSE (DuplicateDetector::isDuplicateId (groups, 2));
}
