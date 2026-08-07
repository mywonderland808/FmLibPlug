#include "sysex/SysexParser.h"
#include <cstring>
#include <fstream>

namespace fmlib
{
namespace
{

bool looksLikeHeaderlessBank (size_t size)
{
    return size == static_cast<size_t> (kPackedBankBytes);
}

void appendUnpackedBank (ParseResult& result, const uint8_t* packed4096)
{
    for (int i = 0; i < kBankVoiceCount; ++i)
    {
        PackedVoice packed {};
        std::memcpy (packed.data(), packed4096 + i * kPackedVoiceBytes, kPackedVoiceBytes);
        ParsedVoice pv;
        pv.data = unpackVoice (packed);
        pv.bankSlot = i + 1;
        result.voices.push_back (pv);
    }
}

void handleYamahaMessage (ParseResult& result, const uint8_t* msg, size_t len)
{
    // F0 43 ch format ... F7
    if (len < 7 || msg[0] != 0xf0 || msg[len - 1] != 0xf7)
    {
        ++result.skipped;
        return;
    }
    if (msg[1] != kYamahaId)
    {
        ++result.skipped;
        return;
    }

    const auto format = msg[3];
    ++result.messagesFound;

    if (format == kFormatSingleVoice)
    {
        // F0 43 0n 00 01 1B [155] cs F7  => len 163
        if (len < 163)
        {
            ++result.skipped;
            return;
        }
        const uint8_t* data = msg + 6;
        const uint8_t cs = msg[6 + kVoiceDataBytes];
        if (! yamahaChecksumOk (data, kVoiceDataBytes, cs))
        {
            ++result.skipped;
            return;
        }
        ParsedVoice pv;
        std::memcpy (pv.data.data(), data, kVoiceDataBytes);
        result.voices.push_back (pv);
        return;
    }

    if (format == kFormatVoiceBank)
    {
        // F0 43 0n 09 20 00 [4096] cs F7 => len 4104
        if (len < 4104)
        {
            ++result.skipped;
            return;
        }
        const uint8_t* data = msg + 6;
        const uint8_t cs = msg[6 + kPackedBankBytes];
        if (! yamahaChecksumOk (data, kPackedBankBytes, cs))
        {
            ++result.skipped;
            return;
        }
        appendUnpackedBank (result, data);
        return;
    }

    ++result.skipped;
}

} // namespace

ParseResult SysexParser::parseBytes (const std::vector<uint8_t>& data)
{
    return parseBytes (data.data(), data.size());
}

ParseResult SysexParser::parseBytes (const uint8_t* data, size_t size)
{
    ParseResult result;
    if (data == nullptr || size == 0)
    {
        result.error = "empty";
        return result;
    }

    if (looksLikeHeaderlessBank (size))
    {
        appendUnpackedBank (result, data);
        result.messagesFound = 1;
        return result;
    }

    size_t i = 0;
    while (i < size)
    {
        while (i < size && data[i] != 0xf0)
            ++i;
        if (i >= size)
            break;
        size_t end = i + 1;
        while (end < size && data[end] != 0xf7)
            ++end;
        if (end >= size)
        {
            result.error = "incomplete sysex";
            break;
        }
        handleYamahaMessage (result, data + i, end - i + 1);
        i = end + 1;
    }

    return result;
}

ParseResult SysexParser::parseFile (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
    {
        ParseResult r;
        r.error = "cannot open";
        return r;
    }
    std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    return parseBytes (bytes);
}

} // namespace fmlib
