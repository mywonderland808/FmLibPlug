#include "sysex/VoiceMorpher.h"
#include "sysex/MorphLocks.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fmlib
{

namespace
{
enum class InterpPolicy : uint8_t
{
    linear = 0,
    nearest = 1,
};

InterpPolicy policyForIndex (size_t i)
{
    // Name bytes are written separately.
    if (i >= 145 && i < 145 + static_cast<size_t> (kNameLength))
        return InterpPolicy::nearest;

    // Global discrete
    if (i == 134) return InterpPolicy::nearest; // algorithm
    if (i == 136) return InterpPolicy::nearest; // osc key sync
    if (i == 141) return InterpPolicy::nearest; // LFO sync
    if (i == 142) return InterpPolicy::nearest; // LFO wave
    if (i == 144) return InterpPolicy::nearest; // transpose

    if (i < 126)
    {
        const int local = static_cast<int> (i % 21);
        if (local == 11 || local == 12) return InterpPolicy::nearest; // kbd curves
        if (local == 17) return InterpPolicy::nearest; // osc mode
        if (local == 18) return InterpPolicy::nearest; // coarse
    }
    return InterpPolicy::linear;
}

uint8_t lerpByte (uint8_t a, uint8_t b, float t)
{
    const float v = static_cast<float> (a) + (static_cast<float> (b) - static_cast<float> (a)) * t;
    return static_cast<uint8_t> (std::clamp (static_cast<int> (std::lround (v)), 0, 255));
}

uint8_t nearestByte (uint8_t a, uint8_t b, float t)
{
    return t < 0.5f ? a : b;
}

uint8_t morphByte (uint8_t a, uint8_t b, float t, InterpPolicy p)
{
    return p == InterpPolicy::nearest ? nearestByte (a, b, t) : lerpByte (a, b, t);
}

char firstNameChar (const VoiceData& v)
{
    for (int i = 0; i < kNameLength; ++i)
    {
        const auto c = static_cast<char> (v[static_cast<size_t> (145 + i)] & 0x7f);
        if (c > 32 && c < 127)
            return c;
    }
    return '-';
}

void writeMorphName (VoiceData& out, const VoiceData& a, const VoiceData& b,
                     const VoiceData& c, const VoiceData& d, float x, float y)
{
    const int xi = std::clamp (static_cast<int> (std::lround (x * 99.0f)), 0, 99);
    const int yi = std::clamp (static_cast<int> (std::lround (y * 99.0f)), 0, 99);

    char name[kNameLength + 1];
    std::snprintf (name, sizeof (name), "%c%c%c%c-%02d:%02d",
                   firstNameChar (a), firstNameChar (b), firstNameChar (c), firstNameChar (d),
                   xi, yi);

    for (int i = 0; i < kNameLength; ++i)
        out[static_cast<size_t> (145 + i)] = static_cast<uint8_t> (name[i] & 0x7f);
}

/** OP1 is a carrier in every DX7 algorithm, so raising its output level always sounds. */
void applySilenceGuard (VoiceData& out, const VoiceData& a, const VoiceData& b,
                        const VoiceData& c, const VoiceData& d)
{
    // If every operator output level in the morph is 0 but at least one corner
    // had a sounding operator, lift the quietest non-zero corner level onto op1.
    bool anyOut = false;
    for (int op = 0; op < 6; ++op)
        if (out[static_cast<size_t> (op * 21 + 16)] > 0)
            anyOut = true;
    if (anyOut)
        return;

    uint8_t floor = 0;
    for (const VoiceData* v : { &a, &b, &c, &d })
        for (int op = 0; op < 6; ++op)
        {
            const auto lvl = (*v)[static_cast<size_t> (op * 21 + 16)];
            if (lvl > 0 && (floor == 0 || lvl < floor))
                floor = lvl;
        }
    if (floor > 0)
        out[16] = floor; // OP1 output level
}

VoiceData morphUnlocked (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                         float x, float y)
{
    const float tx = std::clamp (x, 0.0f, 1.0f);
    const float ty = std::clamp (y, 0.0f, 1.0f);

    VoiceData out {};
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i >= 145 && i < 145 + static_cast<size_t> (kNameLength))
            continue;
        const auto pol = policyForIndex (i);
        const auto top = morphByte (a[i], b[i], tx, pol);
        const auto bot = morphByte (c[i], d[i], tx, pol);
        out[i] = morphByte (top, bot, ty, pol);
    }

    applySilenceGuard (out, a, b, c, d);
    writeMorphName (out, a, b, c, d, tx, ty);
    return out;
}
} // namespace

VoiceData VoiceMorpher::morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                                float x, float y, uint32_t lockGroups, float lockRefX, float lockRefY)
{
    auto out = morphUnlocked (a, b, c, d, x, y);
    if (lockGroups != 0)
    {
        const auto ref = morphUnlocked (a, b, c, d, lockRefX, lockRefY);
        applyMorphLocks (out, ref, lockGroups);
    }
    return out;
}

} // namespace fmlib
