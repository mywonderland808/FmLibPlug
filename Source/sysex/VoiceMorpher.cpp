#include "sysex/VoiceMorpher.h"
#include <algorithm>
#include <cmath>

namespace fmlib
{

namespace
{
uint8_t lerpByte (uint8_t a, uint8_t b, float t)
{
    const float v = static_cast<float> (a) + (static_cast<float> (b) - static_cast<float> (a)) * t;
    return static_cast<uint8_t> (std::clamp (static_cast<int> (std::lround (v)), 0, 255));
}

void copyName (VoiceData& out, const VoiceData& src)
{
    const size_t nameStart = 145;
    for (size_t i = 0; i < static_cast<size_t> (kNameLength); ++i)
        out[nameStart + i] = src[nameStart + i];
}
} // namespace

VoiceData VoiceMorpher::morph4 (const VoiceData& a, const VoiceData& b, const VoiceData& c, const VoiceData& d,
                                float x, float y)
{
    const float tx = std::clamp (x, 0.0f, 1.0f);
    const float ty = std::clamp (y, 0.0f, 1.0f);

    VoiceData out {};
    for (size_t i = 0; i < out.size(); ++i)
    {
        const auto top = lerpByte (a[i], b[i], tx);
        const auto bot = lerpByte (c[i], d[i], tx);
        out[i] = lerpByte (top, bot, ty);
    }

    const VoiceData* nearest = &a;
    float best = tx * tx + ty * ty;
    const float dB = (1 - tx) * (1 - tx) + ty * ty;
    const float dC = tx * tx + (1 - ty) * (1 - ty);
    const float dD = (1 - tx) * (1 - tx) + (1 - ty) * (1 - ty);
    if (dB < best) { best = dB; nearest = &b; }
    if (dC < best) { best = dC; nearest = &c; }
    if (dD < best) { nearest = &d; }
    copyName (out, *nearest);
    return out;
}

} // namespace fmlib
