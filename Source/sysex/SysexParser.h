#pragma once

#include "sysex/Dx7Formats.h"
#include <filesystem>
#include <vector>

namespace fmlib
{

struct ParsedVoice
{
    VoiceData data {};
    int bankSlot = -1; // 1..32 when from a bank, else -1
};

struct ParseResult
{
    std::vector<ParsedVoice> voices;
    int messagesFound = 0;
    int skipped = 0;
    std::string error;
};

class SysexParser
{
public:
    static ParseResult parseFile (const std::filesystem::path& path);
    static ParseResult parseBytes (const uint8_t* data, size_t size);
    static ParseResult parseBytes (const std::vector<uint8_t>& data);
};

} // namespace fmlib
