#include "sysex/FormatDetect.h"
#include "sysex/Tx7Function.h"
#include "util/StringUtils.h"
#include <fstream>

namespace fmlib
{

namespace
{
std::string lowerExt (const std::filesystem::path& p)
{
    return asciiLower (p.extension().string());
}

bool isDx7iiFormatByte (uint8_t format)
{
    // DX7II / TX802 additional voice formats (not mkI 0x00 / 0x09)
    return format == 0x05 || format == 0x06 || format == 0x7e;
}
} // namespace

FormatDetectResult FormatDetect::detect (const uint8_t* data, size_t size)
{
    FormatDetectResult r;
    if (data == nullptr || size == 0)
    {
        r.label = "empty";
        return r;
    }

    if (size == static_cast<size_t> (kPackedBankBytes))
    {
        r.kind = SysexFormatKind::dx7HeaderlessBank;
        r.label = "DX7 headerless bank";
        r.supported = true;
        return r;
    }

    // Dexed .dx7 raw packed bank is often exactly 4096; already handled.
    // Some .dx7 files are 4104 with sysex wrapper — handled by Yamaha path.

    if (size >= 7 && data[0] == 0xf0 && data[1] == kYamahaId)
    {
        const uint8_t format = data[3];
        if (format == kFormatSingleVoice)
        {
            r.kind = SysexFormatKind::dx7Single;
            r.label = "DX7 single voice";
            r.supported = true;
            return r;
        }
        if (format == kFormatVoiceBank)
        {
            r.kind = SysexFormatKind::dx7Bank;
            r.label = "DX7 32-voice bank";
            r.supported = true;
            return r;
        }
        if (isDx7iiFormatByte (format))
        {
            r.kind = SysexFormatKind::dx7iiOrTx802;
            r.label = "DX7II/TX802 (unsupported skip)";
            r.supported = false;
            return r;
        }
        if (format == kFormatPerformance)
        {
            r.kind = SysexFormatKind::functionDump;
            r.label = "TX7 1-performance / function";
            // Not a voice library format — FunctionBuffer parses via Tx7Performance.
            r.supported = false;
            return r;
        }
        if (format == kFormatPerformanceBank)
        {
            r.kind = SysexFormatKind::functionDump;
            r.label = "TX7 64-performance bank (unsupported)";
            r.supported = false;
            return r;
        }
    }

    // Dexed single packed voice: 128 bytes raw
    if (size == static_cast<size_t> (kPackedVoiceBytes))
    {
        r.kind = SysexFormatKind::dexedDx7;
        r.label = "Dexed packed voice";
        r.supported = true;
        return r;
    }

    r.label = "unknown";
    return r;
}

FormatDetectResult FormatDetect::detectFile (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
    {
        FormatDetectResult r;
        r.label = "unreadable";
        return r;
    }
    std::vector<uint8_t> buf ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    auto r = detect (buf.data(), buf.size());
    if (lowerExt (path) == ".dx7" && r.kind == SysexFormatKind::unknown && buf.size() == static_cast<size_t> (kPackedBankBytes))
    {
        r.kind = SysexFormatKind::dexedDx7;
        r.label = "Dexed .dx7 bank";
        r.supported = true;
    }
    else if (lowerExt (path) == ".dx7" && (r.kind == SysexFormatKind::dx7HeaderlessBank || r.kind == SysexFormatKind::dexedDx7))
    {
        r.kind = SysexFormatKind::dexedDx7;
        r.label = "Dexed .dx7";
        r.supported = true;
    }
    return r;
}

ParseResult FormatDetect::parseSupported (const uint8_t* data, size_t size)
{
    auto det = detect (data, size);
    if (! det.supported)
    {
        ParseResult result;
        result.error = det.label.empty() ? "unsupported format" : det.label;
        ++result.skipped;
        return result;
    }

    return SysexParser::parseBytes (data, size);
}

} // namespace fmlib
