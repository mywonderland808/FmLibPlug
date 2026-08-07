#include "sysex/FunctionDump.h"

namespace fmlib
{

FunctionDumpInfo FunctionDump::inspect (const uint8_t* data, size_t size)
{
    FunctionDumpInfo info;
    info.byteLength = size;
    if (data == nullptr || size < 7 || data[0] != 0xf0 || data[1] != kYamahaId)
        return info;

    const uint8_t format = data[3];
    // TX7 function parameters often use bulk dumps distinct from voice 0x00/0x09
    if (format == 0x01 || format == 0x02 || format == 0x04)
    {
        info.recognized = true;
        info.label = format == 0x01 ? "TX7 function-like dump" : "TX7 performance-like dump";
    }
    return info;
}

bool FunctionDump::isFunctionOrPerformance (const uint8_t* data, size_t size)
{
    return inspect (data, size).recognized;
}

} // namespace fmlib
