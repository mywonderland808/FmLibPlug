#include "library/AutoTagger.h"
#include "util/StringUtils.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fmlib
{

namespace
{
void addUnique (std::vector<std::string>& tags, const char* t)
{
    const std::string s (t);
    if (std::find (tags.begin(), tags.end(), s) == tags.end())
        tags.push_back (s);
}

bool isLetter (char c)
{
    return std::isalpha (static_cast<unsigned char> (c)) != 0;
}

/** Substring match; short needles (<=3) need a token boundary.
    Boundaries: start/end, non-letter, or CamelCase (DarkPad -> pad). */
bool matches (const std::string& original, const std::string& hay, const char* needle)
{
    const auto nlen = std::strlen (needle);
    if (nlen == 0)
        return false;
    size_t pos = 0;
    while ((pos = hay.find (needle, pos)) != std::string::npos)
    {
        if (nlen > 3)
            return true;
        const bool camelLeft = pos < original.size()
                               && std::isupper (static_cast<unsigned char> (original[pos])) != 0;
        const bool camelRight = pos + nlen < original.size()
                                && std::isupper (static_cast<unsigned char> (original[pos + nlen])) != 0;
        const bool leftOk = pos == 0 || ! isLetter (hay[pos - 1]) || camelLeft;
        const bool rightOk = pos + nlen >= hay.size() || ! isLetter (hay[pos + nlen]) || camelRight;
        if (leftOk && rightOk)
            return true;
        ++pos;
    }
    return false;
}
} // namespace

std::vector<std::string> AutoTagger::tagsForVoice (const VoiceData&, const std::string& voiceName)
{
    std::vector<std::string> tags;
    const auto name = asciiLower (voiceName);
    if (name.empty())
        return tags;

    struct Pair { const char* needle; const char* tag; };
    // Longer needles first where overlapping (string before str, piano before pno handled by length rule).
    static constexpr Pair categories[] = {
        { "string", "string" }, { "strgs", "string" }, { "strs", "string" },
        { "violin", "string" }, { "cello", "string" }, { "viola", "string" },
        { "fiddle", "string" }, { "orchestra", "string" }, { "str", "string" },
        { "pad", "pad" },
        { "bass", "bass" },
        { "synth", "synth" },
        { "effect", "fx" }, { "noise", "fx" }, { "atmo", "fx" }, { "sfx", "fx" }, { "fx", "fx" },
        { "brass", "brass" }, { "trumpet", "brass" }, { "trombo", "brass" },
        { "horn", "brass" }, { "tuba", "brass" },
        { "epiano", "keys" }, { "e-piano", "keys" }, { "piano", "keys" }, { "grand", "keys" },
        { "rhodes", "keys" }, { "wurli", "keys" }, { "harpsi", "keys" }, { "clav", "keys" },
        { "keys", "keys" }, { "pno", "keys" },
        { "lead", "lead" }, { "solo", "lead" }, { "ld", "lead" },
        { "organ", "organ" }, { "hammond", "organ" }, { "org", "organ" },
        { "harmonica", "wind" }, { "clarinet", "wind" }, { "flute", "flute" },
        { "accord", "wind" }, { "oboe", "wind" }, { "sax", "wind" }, { "wind", "wind" },
        { "celest", "bell" }, { "marimba", "bell" }, { "kalimba", "bell" },
        { "chime", "bell" }, { "vibra", "bell" }, { "xylo", "bell" }, { "bell", "bell" },
        { "mandolin", "guitar" }, { "ukulele", "guitar" }, { "guitar", "guitar" },
        { "banjo", "guitar" }, { "sitar", "guitar" }, { "harp", "guitar" },
        { "lute", "guitar" }, { "gtr", "guitar" }, { "uke", "guitar" }, { "pluck", "pluck" },
        { "snare", "perc" }, { "drum", "perc" }, { "perc", "perc" }, { "perk", "perc" },
        { "kick", "perc" }, { "timp", "perc" }, { "cym", "perc" },
        { "vocal", "vocal" }, { "choir", "vocal" }, { "voice", "vocal" }, { "vox", "vocal" },
    };

    for (const auto& p : categories)
    {
        if (matches (voiceName, name, p.needle))
            addUnique (tags, p.tag);
    }

    static constexpr Pair moods[] = {
        { "dark", "dark" }, { "mellow", "mellow" }, { "bright", "bright" },
        { "soft", "soft" }, { "hard", "hard" }, { "warm", "warm" }, { "cold", "cold" },
        { "fat", "fat" }, { "thin", "thin" }, { "deep", "deep" },
        { "heavy", "heavy" }, { "clean", "clean" }, { "dirty", "dirty" },
        { "funky", "funky" }, { "ambient", "ambient" }, { "eerie", "eerie" },
        { "crystal", "crystal" }, { "glass", "glass" }, { "metal", "metal" },
        { "sweet", "sweet" }, { "sharp", "sharp" }, { "smooth", "smooth" },
        { "space", "space" }, { "magic", "magic" }, { "analog", "analog" }, { "digital", "digital" },
    };

    for (const auto& p : moods)
    {
        if (matches (voiceName, name, p.needle))
            addUnique (tags, p.tag);
    }

    return tags;
}

} // namespace fmlib
