#include "sysex/Dx7Formats.h"

namespace fmlib
{

std::string voiceNameFromData (const VoiceData& voice)
{
    std::string name;
    name.reserve (kNameLength);
    for (int i = 0; i < kNameLength; ++i)
    {
        const auto c = static_cast<char> (voice[static_cast<size_t> (145 + i)] & 0x7f);
        name.push_back ((c >= 32 && c < 127) ? c : ' ');
    }
    while (! name.empty() && name.back() == ' ')
        name.pop_back();
    if (name.empty())
        name = "(unnamed)";
    return name;
}

uint64_t contentIdFromVoice (const VoiceData& voice)
{
    uint64_t hash = 14695981039346656037ull;
    for (auto b : voice)
    {
        hash ^= static_cast<uint64_t> (b);
        hash *= 1099511628211ull;
    }
    return hash;
}

VoiceData unpackVoice (const PackedVoice& packed)
{
    // VCED order per op: EG(8)+BP/LD/RD(3)+LC+RC+RS+AMS+KVS+OL+MODE+COARSE+FINE+DETUNE (21)
    // Packed byte12 holds RS|DET together; DETUNE is last in VCED.
    VoiceData u {};
    int up = 0;

    for (int op = 0; op < 6; ++op)
    {
        const int p = op * 17;
        for (int i = 0; i < 11; ++i)
            u[static_cast<size_t> (up++)] = packed[static_cast<size_t> (p + i)];

        const auto b11 = packed[static_cast<size_t> (p + 11)];
        u[static_cast<size_t> (up++)] = b11 & 0x03;        // left curve
        u[static_cast<size_t> (up++)] = (b11 >> 2) & 0x03; // right curve

        const auto b12 = packed[static_cast<size_t> (p + 12)];
        u[static_cast<size_t> (up++)] = b12 & 0x07; // rate scaling
        const auto detune = static_cast<uint8_t> ((b12 >> 3) & 0x0f);

        const auto b13 = packed[static_cast<size_t> (p + 13)];
        u[static_cast<size_t> (up++)] = b13 & 0x03;        // amp mod sens
        u[static_cast<size_t> (up++)] = (b13 >> 2) & 0x07; // key vel sens

        u[static_cast<size_t> (up++)] = packed[static_cast<size_t> (p + 14)]; // output level

        const auto b15 = packed[static_cast<size_t> (p + 15)];
        u[static_cast<size_t> (up++)] = b15 & 0x01;        // osc mode
        u[static_cast<size_t> (up++)] = (b15 >> 1) & 0x1f; // coarse
        u[static_cast<size_t> (up++)] = packed[static_cast<size_t> (p + 16)]; // fine
        u[static_cast<size_t> (up++)] = detune;            // detune (VCED last)
    }

    for (int i = 0; i < 8; ++i)
        u[static_cast<size_t> (126 + i)] = packed[static_cast<size_t> (102 + i)];

    u[134] = packed[110] & 0x1f;                         // algorithm
    u[135] = packed[111] & 0x07;                         // feedback
    u[136] = static_cast<uint8_t> ((packed[111] >> 3) & 0x01); // osc key sync
    u[137] = packed[112];
    u[138] = packed[113];
    u[139] = packed[114];
    u[140] = packed[115];
    u[141] = packed[116] & 0x01;                         // LFO sync
    u[142] = (packed[116] >> 1) & 0x07;                  // LFO wave
    u[143] = (packed[116] >> 4) & 0x07;                  // pitch mod sens
    u[144] = packed[117];                                // transpose

    for (int i = 0; i < 10; ++i)
        u[static_cast<size_t> (145 + i)] = packed[static_cast<size_t> (118 + i)];

    return u;
}

PackedVoice packVoice (const VoiceData& u)
{
    PackedVoice p {};
    int up = 0;

    for (int op = 0; op < 6; ++op)
    {
        const int base = op * 17;
        for (int i = 0; i < 11; ++i)
            p[static_cast<size_t> (base + i)] = u[static_cast<size_t> (up++)];

        const auto lc = u[static_cast<size_t> (up++)] & 0x03;
        const auto rc = u[static_cast<size_t> (up++)] & 0x03;
        p[static_cast<size_t> (base + 11)] = static_cast<uint8_t> (lc | (rc << 2));

        const auto rs = u[static_cast<size_t> (up++)] & 0x07;
        const auto ams = u[static_cast<size_t> (up++)] & 0x03;
        const auto kvs = u[static_cast<size_t> (up++)] & 0x07;
        p[static_cast<size_t> (base + 13)] = static_cast<uint8_t> (ams | (kvs << 2));

        p[static_cast<size_t> (base + 14)] = u[static_cast<size_t> (up++)];
        const auto mode = u[static_cast<size_t> (up++)] & 0x01;
        const auto coarse = u[static_cast<size_t> (up++)] & 0x1f;
        p[static_cast<size_t> (base + 15)] = static_cast<uint8_t> (mode | (coarse << 1));
        p[static_cast<size_t> (base + 16)] = u[static_cast<size_t> (up++)];
        const auto det = u[static_cast<size_t> (up++)] & 0x0f;
        p[static_cast<size_t> (base + 12)] = static_cast<uint8_t> (rs | (det << 3));
    }

    for (int i = 0; i < 8; ++i)
        p[static_cast<size_t> (102 + i)] = u[static_cast<size_t> (126 + i)];

    p[110] = static_cast<uint8_t> (u[134] & 0x1f);
    p[111] = static_cast<uint8_t> ((u[135] & 0x07) | ((u[136] & 0x01) << 3));
    p[112] = u[137];
    p[113] = u[138];
    p[114] = u[139];
    p[115] = u[140];
    p[116] = static_cast<uint8_t> ((u[141] & 0x01) | ((u[142] & 0x07) << 1) | ((u[143] & 0x07) << 4));
    p[117] = u[144];

    for (int i = 0; i < 10; ++i)
        p[static_cast<size_t> (118 + i)] = u[static_cast<size_t> (145 + i)];

    return p;
}

} // namespace fmlib
