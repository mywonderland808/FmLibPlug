#pragma once

#include "sysex/Dx7Formats.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace fmlib
{

constexpr int kPerformanceDataBytes = 94;
constexpr uint8_t kFormatPerformance = 0x01;     // 1 performance / function edit buffer
constexpr uint8_t kFormatPerformanceBank = 0x02; // 64 performances (unsupported in v1.3.5)

/** TX7 1-performance bulk payload (94 bytes). Voice-A fields used by the Globals UI. */
using Tx7PerformanceData = std::array<uint8_t, kPerformanceDataBytes>;

/** DX7/TX7 function parameter numbers for g=2 parameter-change messages. */
enum class DxFunctionParam : uint8_t
{
    polyMono = 64,       // 0 = poly, 1 = mono
    pitchBendRange = 65,
    pitchBendStep = 66,
    portamentoMode = 67,
    portamentoGliss = 68,
    portamentoTime = 69,
    modWheelSensitivity = 70, // live 0-99; TX7 stores 0-15 in bulk
    modWheelAssign = 71,      // bit0 pitch, bit1 amp, bit2 EG bias
    footSensitivity = 72,
    footAssign = 73,
    breathSensitivity = 74,
    breathAssign = 75,
    aftertouchSensitivity = 76,
    aftertouchAssign = 77,
};

/** TX7-only function parameters for g=4, h=1 parameter-change messages. */
enum class TxFunctionParam : uint8_t
{
    dataEntryReceive = 0,      // 0/1
    controlChangeReceive = 1,  // 0/1
    dataEntryVolume = 2,       // 0/1
    computeCommunication = 3,  // 0/1
    combinedOrIndividual = 4,  // 0 = combined, 1 = individual
    noteLimitLow = 5,          // 0-127
    noteLimitHigh = 6,         // 0-127
    memoryProtect = 7,         // 0 = off, 127 = on
    loadFunctionSelect = 11,   // 0 = int, 127 = ext
};

/** Yamaha parameter-change group byte: 0ggggghh (g = group, h = high bits / machine). */
inline uint8_t yamahaParamGroupByte (uint8_t group, uint8_t h = 0)
{
    return static_cast<uint8_t> (((group & 0x1f) << 2) | (h & 0x03));
}

namespace Tx7Performance
{
/**
 * Voice-A field indices inside the 94-byte 1-performance bulk (TX7 §4-6).
 * Voice B half is preserved on Get/Send; editing is a future TODO.
 * Some OCR skips printing index 7 between gliss (6) and blank (8); Yamaha
 * lists portamento mode there (also DX g=2 param 67).
 */
constexpr int kPolyMono = 2;
constexpr int kPitchBendRange = 3;
constexpr int kPitchBendStep = 4;
constexpr int kPortamentoTime = 5;
constexpr int kPortamentoGliss = 6;
constexpr int kPortamentoMode = 7;
constexpr int kModWheelSensitivity = 9;  // 0-15
constexpr int kModWheelAssign = 10;      // 0-7 bitfield P/A/E
constexpr int kFootSensitivity = 11;
constexpr int kFootAssign = 12;
constexpr int kAftertouchSensitivity = 13;
constexpr int kAftertouchAssign = 14;
constexpr int kBreathSensitivity = 15;
constexpr int kBreathAssign = 16;
constexpr int kAttenuator = 26; // 0-7

constexpr uint8_t kAssignPitch = 1 << 0;
constexpr uint8_t kAssignAmp = 1 << 1;
constexpr uint8_t kAssignEgBias = 1 << 2;

inline uint8_t clampU8 (uint8_t v, uint8_t lo, uint8_t hi)
{
    return static_cast<uint8_t> (std::min (hi, std::max (lo, v)));
}

inline uint8_t get (const Tx7PerformanceData& d, int index)
{
    if (index < 0 || index >= kPerformanceDataBytes)
        return 0;
    return d[static_cast<size_t> (index)] & 0x7f;
}

inline void set (Tx7PerformanceData& d, int index, uint8_t value)
{
    if (index < 0 || index >= kPerformanceDataBytes)
        return;
    d[static_cast<size_t> (index)] = value & 0x7f;
}

inline uint8_t polyMono (const Tx7PerformanceData& d) { return get (d, kPolyMono); }
inline uint8_t pitchBendRange (const Tx7PerformanceData& d) { return get (d, kPitchBendRange); }
inline uint8_t pitchBendStep (const Tx7PerformanceData& d) { return get (d, kPitchBendStep); }
inline uint8_t portamentoTime (const Tx7PerformanceData& d) { return get (d, kPortamentoTime); }
inline uint8_t portamentoGliss (const Tx7PerformanceData& d) { return get (d, kPortamentoGliss); }
inline uint8_t portamentoMode (const Tx7PerformanceData& d) { return get (d, kPortamentoMode); }
inline uint8_t modWheelSensitivity (const Tx7PerformanceData& d) { return get (d, kModWheelSensitivity); }
inline uint8_t modWheelAssign (const Tx7PerformanceData& d) { return get (d, kModWheelAssign) & 0x07; }
inline uint8_t footSensitivity (const Tx7PerformanceData& d) { return get (d, kFootSensitivity); }
inline uint8_t footAssign (const Tx7PerformanceData& d) { return get (d, kFootAssign) & 0x07; }
inline uint8_t aftertouchSensitivity (const Tx7PerformanceData& d) { return get (d, kAftertouchSensitivity); }
inline uint8_t aftertouchAssign (const Tx7PerformanceData& d) { return get (d, kAftertouchAssign) & 0x07; }
inline uint8_t breathSensitivity (const Tx7PerformanceData& d) { return get (d, kBreathSensitivity); }
inline uint8_t breathAssign (const Tx7PerformanceData& d) { return get (d, kBreathAssign) & 0x07; }
inline uint8_t attenuator (const Tx7PerformanceData& d) { return get (d, kAttenuator); }

/** Map bulk controller sensitivity (0-15) to DX g=2 live value (0-99). */
inline uint8_t controllerSensitivityToLive (uint8_t stored0to15)
{
    const auto s = clampU8 (stored0to15, 0, 15);
    return static_cast<uint8_t> ((static_cast<int> (s) * 99 + 7) / 15);
}

inline void setPolyMono (Tx7PerformanceData& d, uint8_t v) { set (d, kPolyMono, v > 0 ? 1 : 0); }
inline void setPitchBendRange (Tx7PerformanceData& d, uint8_t v) { set (d, kPitchBendRange, clampU8 (v, 0, 12)); }
inline void setPitchBendStep (Tx7PerformanceData& d, uint8_t v) { set (d, kPitchBendStep, clampU8 (v, 0, 12)); }
inline void setPortamentoTime (Tx7PerformanceData& d, uint8_t v) { set (d, kPortamentoTime, clampU8 (v, 0, 99)); }
inline void setPortamentoGliss (Tx7PerformanceData& d, uint8_t v) { set (d, kPortamentoGliss, v > 0 ? 1 : 0); }
inline void setPortamentoMode (Tx7PerformanceData& d, uint8_t v) { set (d, kPortamentoMode, v > 0 ? 1 : 0); }
inline void setModWheelSensitivity (Tx7PerformanceData& d, uint8_t v)
{
    set (d, kModWheelSensitivity, clampU8 (v, 0, 15));
}
inline void setModWheelAssign (Tx7PerformanceData& d, uint8_t v) { set (d, kModWheelAssign, v & 0x07); }
inline void setFootSensitivity (Tx7PerformanceData& d, uint8_t v)
{
    set (d, kFootSensitivity, clampU8 (v, 0, 15));
}
inline void setFootAssign (Tx7PerformanceData& d, uint8_t v) { set (d, kFootAssign, v & 0x07); }
inline void setAftertouchSensitivity (Tx7PerformanceData& d, uint8_t v)
{
    set (d, kAftertouchSensitivity, clampU8 (v, 0, 15));
}
inline void setAftertouchAssign (Tx7PerformanceData& d, uint8_t v) { set (d, kAftertouchAssign, v & 0x07); }
inline void setBreathSensitivity (Tx7PerformanceData& d, uint8_t v)
{
    set (d, kBreathSensitivity, clampU8 (v, 0, 15));
}
inline void setBreathAssign (Tx7PerformanceData& d, uint8_t v) { set (d, kBreathAssign, v & 0x07); }
inline void setAttenuator (Tx7PerformanceData& d, uint8_t v) { set (d, kAttenuator, clampU8 (v, 0, 7)); }

/** Clamp and write a known Voice-A field by bulk index (unknown indices: raw 7-bit). */
inline void setVoiceAField (Tx7PerformanceData& d, int index, uint8_t value)
{
    switch (index)
    {
        case kPolyMono: setPolyMono (d, value); break;
        case kPitchBendRange: setPitchBendRange (d, value); break;
        case kPitchBendStep: setPitchBendStep (d, value); break;
        case kPortamentoTime: setPortamentoTime (d, value); break;
        case kPortamentoGliss: setPortamentoGliss (d, value); break;
        case kPortamentoMode: setPortamentoMode (d, value); break;
        case kModWheelSensitivity: setModWheelSensitivity (d, value); break;
        case kModWheelAssign: setModWheelAssign (d, value); break;
        case kFootSensitivity: setFootSensitivity (d, value); break;
        case kFootAssign: setFootAssign (d, value); break;
        case kAftertouchSensitivity: setAftertouchSensitivity (d, value); break;
        case kAftertouchAssign: setAftertouchAssign (d, value); break;
        case kBreathSensitivity: setBreathSensitivity (d, value); break;
        case kBreathAssign: setBreathAssign (d, value); break;
        case kAttenuator: setAttenuator (d, value); break;
        default: set (d, index, value); break;
    }
}

/** Sensible defaults matching TX7 factory function init (service manual). */
Tx7PerformanceData makeDefault();

/** Set Voice-A editable fields to Yamaha factory function defaults; leaves the rest of the dump intact. */
void applyVoiceADefaults (Tx7PerformanceData& d);

/**
 * True when header/length match a 1-performance bulk (checksum not checked).
 * Use before parse to distinguish corrupt dumps from unrelated SysEx.
 */
bool looksLikePerformanceBulk (const uint8_t* data, size_t size);

/**
 * Parse a full SysEx message (or raw 94-byte payload) into performance data.
 * Accepts F0 43 0n 01 00 5E [94] cs F7.
 */
std::optional<Tx7PerformanceData> parsePerformanceBulk (const uint8_t* data, size_t size);
inline std::optional<Tx7PerformanceData> parsePerformanceBulk (const std::vector<uint8_t>& bytes)
{
    return parsePerformanceBulk (bytes.data(), bytes.size());
}

/** True when bytes are a valid Yamaha 1-performance bulk (format 0x01 + checksum). */
bool isPerformanceBulkMessage (const uint8_t* data, size_t size);
} // namespace Tx7Performance

} // namespace fmlib
