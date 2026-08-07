#pragma once

#include "sysex/Dx7Formats.h"
#include <optional>
#include <string>
#include <vector>

namespace fmlib
{

struct FunctionDumpInfo
{
    bool recognized = false;
    std::string label;
    size_t byteLength = 0;
};

/** TX7 function / performance dump helpers (detect + light parse; send later). */
class FunctionDump
{
public:
    static FunctionDumpInfo inspect (const uint8_t* data, size_t size);
    static bool isFunctionOrPerformance (const uint8_t* data, size_t size);
};

} // namespace fmlib
