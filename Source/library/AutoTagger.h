#pragma once

#include "sysex/Dx7Formats.h"
#include <string>
#include <vector>

namespace fmlib
{

class AutoTagger
{
public:
    /** Name-based category/mood tags only; empty if nothing can be deduced. */
    static std::vector<std::string> tagsForVoice (const VoiceData& voice, const std::string& voiceName);
};

} // namespace fmlib
