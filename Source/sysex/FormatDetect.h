#pragma once

#include "sysex/Dx7Formats.h"
#include "sysex/SysexParser.h"
#include <filesystem>
#include <string>
#include <vector>

namespace fmlib
{

enum class SysexFormatKind
{
    unknown,
    dx7Single,
    dx7Bank,
    dx7HeaderlessBank,
    dexedDx7,
    dx7iiOrTx802,
    functionDump
};

struct FormatDetectResult
{
    SysexFormatKind kind = SysexFormatKind::unknown;
    std::string label;
    bool supported = false;
};

class FormatDetect
{
public:
    static FormatDetectResult detect (const uint8_t* data, size_t size);
    static FormatDetectResult detectFile (const std::filesystem::path& path);

    /** Parse supported formats into ParseResult; unsupported returns empty with error set. */
    static ParseResult parseSupported (const uint8_t* data, size_t size, const std::filesystem::path& hintPath = {});
};

} // namespace fmlib
