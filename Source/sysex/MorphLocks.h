#pragma once

#include "sysex/Dx7Formats.h"
#include <bitset>
#include <cstdint>

namespace fmlib
{

/** Coarse VCED lock groups for morph (bit flags). */
enum MorphLockGroup : uint32_t
{
    morphLockNone       = 0,
    morphLockEg         = 1u << 0,  // per-op EG rates + levels (0-7)
    morphLockLevels     = 1u << 1,  // per-op output level (16)
    morphLockFreqCoarse = 1u << 2,  // osc mode + coarse (17-18)
    morphLockAlgo       = 1u << 3,  // algorithm (134)
    morphLockPitchEg    = 1u << 4,
    morphLockLfo        = 1u << 5,
    morphLockScaling    = 1u << 6,  // kbd level scale + rate scale (8-13)
    morphLockSens       = 1u << 7,  // AMS + key velocity sens (14-15)
    morphLockTranspose  = 1u << 8,  // transpose (144)
    morphLockFeedback   = 1u << 9,  // feedback (135)
    morphLockOscSync    = 1u << 10, // osc key sync (136)
    morphLockFreqFine   = 1u << 11, // fine + detune (19-20)
    /** @deprecated Alias for both coarse+fine (schema 1 prefs / presets). */
    morphLockFreq       = morphLockFreqCoarse | morphLockFreqFine,
};

/** Bits that are valid lock groups. */
inline constexpr uint32_t morphLockAllGroups = morphLockEg | morphLockLevels | morphLockFreqCoarse
                                            | morphLockAlgo | morphLockPitchEg | morphLockLfo
                                            | morphLockScaling | morphLockSens | morphLockTranspose
                                            | morphLockFeedback | morphLockOscSync | morphLockFreqFine;

/**
 * Factory defaults: keep envelope + levels at the lock reference so a morph
 * cannot go dead; structural/freq params stay morphable. Clicks from those are
 * limited by the active MorphStreamMode, not by locking.
 */
inline constexpr uint32_t morphLockFactoryDefaults = morphLockEg | morphLockLevels;

std::bitset<kVoiceDataBytes> morphLockMask (uint32_t groups);

/** Copy locked bytes from reference into voice. */
void applyMorphLocks (VoiceData& voice, const VoiceData& reference, uint32_t groups);

/** Migrate schema-1 lock bitfields (Freq was a single bit at 1<<2 covering 17-20). */
uint32_t migrateMorphLockGroups (uint32_t groups, int fromSchema);

} // namespace fmlib
