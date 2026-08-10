#include "sysex/MorphLocks.h"

namespace fmlib
{

namespace
{
constexpr int kOpStride = 21;
constexpr int kNumOps = 6;
} // namespace

std::bitset<kVoiceDataBytes> morphLockMask (uint32_t groups)
{
    std::bitset<kVoiceDataBytes> mask;
    groups &= morphLockAllGroups;
    if (groups == 0)
        return mask;

    for (int op = 0; op < kNumOps; ++op)
    {
        const int base = op * kOpStride;
        if ((groups & morphLockEg) != 0)
            for (int i = 0; i < 8; ++i)
                mask.set (static_cast<size_t> (base + i));
        if ((groups & morphLockScaling) != 0)
            for (int i = 8; i <= 13; ++i)
                mask.set (static_cast<size_t> (base + i));
        if ((groups & morphLockSens) != 0)
        {
            mask.set (static_cast<size_t> (base + 14));
            mask.set (static_cast<size_t> (base + 15));
        }
        if ((groups & morphLockLevels) != 0)
            mask.set (static_cast<size_t> (base + 16));
        if ((groups & morphLockFreqCoarse) != 0)
        {
            mask.set (static_cast<size_t> (base + 17));
            mask.set (static_cast<size_t> (base + 18));
        }
        if ((groups & morphLockFreqFine) != 0)
        {
            mask.set (static_cast<size_t> (base + 19));
            mask.set (static_cast<size_t> (base + 20));
        }
    }

    if ((groups & morphLockPitchEg) != 0)
        for (int i = 126; i <= 133; ++i)
            mask.set (static_cast<size_t> (i));
    if ((groups & morphLockAlgo) != 0)
        mask.set (134);
    if ((groups & morphLockFeedback) != 0)
        mask.set (135);
    if ((groups & morphLockOscSync) != 0)
        mask.set (136);
    if ((groups & morphLockLfo) != 0)
        for (int i = 137; i <= 143; ++i)
            mask.set (static_cast<size_t> (i));
    if ((groups & morphLockTranspose) != 0)
        mask.set (144);

    return mask;
}

void applyMorphLocks (VoiceData& voice, const VoiceData& reference, uint32_t groups)
{
    if (groups == 0)
        return;
    const auto mask = morphLockMask (groups);
    for (size_t i = 0; i < voice.size(); ++i)
        if (mask.test (i))
            voice[i] = reference[i];
}

uint32_t migrateMorphLockGroups (uint32_t groups, int fromSchema)
{
    if (fromSchema >= 2)
        return groups & morphLockAllGroups;

    // Schema 0/1: bit 2 meant the full Freq block (mode/coarse/fine/detune).
    // After the split, that bit is morphLockFreqCoarse only — expand to both.
    if ((groups & (1u << 2)) != 0)
        groups |= morphLockFreqFine;
    return groups & morphLockAllGroups;
}

} // namespace fmlib
