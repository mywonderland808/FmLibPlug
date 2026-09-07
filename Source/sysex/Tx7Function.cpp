#include "sysex/Tx7Function.h"

namespace fmlib
{
namespace Tx7System
{

State makeDefault()
{
    // Power-on: Memory Protect ON (TX7 manuals). Note limits C-2..G8 from Voice INIT
    // function table. MIDI mode switches are NVRAM-memorized; shipping/reset uses
    // Combined, CC/DataEntry/Compute off, Load Function INT.
    return {};
}

void applyParam (State& s, TxFunctionParam param, uint8_t value)
{
    const auto v = static_cast<uint8_t> (value & 0x7f);
    switch (param)
    {
        case TxFunctionParam::dataEntryReceive: s.dataEntryReceive = v != 0; break;
        case TxFunctionParam::controlChangeReceive: s.controlChangeReceive = v != 0; break;
        case TxFunctionParam::dataEntryVolume: s.dataEntryVolume = v != 0; break;
        case TxFunctionParam::computeCommunication: s.computeCommunication = v != 0; break;
        case TxFunctionParam::combinedOrIndividual: s.individualMode = v != 0; break;
        case TxFunctionParam::noteLimitLow: s.noteLimitLow = v; break;
        case TxFunctionParam::noteLimitHigh: s.noteLimitHigh = v; break;
        case TxFunctionParam::memoryProtect: s.memoryProtect = v != 0; break;
        case TxFunctionParam::loadFunctionSelect: s.loadFunctionExt = v != 0; break;
        default: break;
    }
    if (param == TxFunctionParam::noteLimitLow || param == TxFunctionParam::noteLimitHigh)
        clampNoteLimits (s);
}

void clampNoteLimits (State& s)
{
    if (s.noteLimitLow > s.noteLimitHigh)
        std::swap (s.noteLimitLow, s.noteLimitHigh);
}

void applyComputeCommunicationOnEffects (State& s)
{
    s.computeCommunication = true;
    s.individualMode = false;
    s.controlChangeReceive = true;
    s.dataEntryReceive = true;
    s.dataEntryVolume = false;
}

std::vector<std::pair<TxFunctionParam, uint8_t>> encodeStateForSend (const State& state)
{
    std::vector<std::pair<TxFunctionParam, uint8_t>> out;
    out.reserve (9);
    out.emplace_back (TxFunctionParam::dataEntryReceive, state.dataEntryReceive ? 1 : 0);
    out.emplace_back (TxFunctionParam::controlChangeReceive, state.controlChangeReceive ? 1 : 0);
    out.emplace_back (TxFunctionParam::dataEntryVolume, state.dataEntryVolume ? 1 : 0);
    // Skip computeCommunication=0 — that value triggers §4-4 Combined/CC/DataEntry + perf dump.
    if (state.computeCommunication)
        out.emplace_back (TxFunctionParam::computeCommunication, 1);
    out.emplace_back (TxFunctionParam::combinedOrIndividual, state.individualMode ? 1 : 0);
    out.emplace_back (TxFunctionParam::noteLimitLow, state.noteLimitLow);
    out.emplace_back (TxFunctionParam::noteLimitHigh, state.noteLimitHigh);
    out.emplace_back (TxFunctionParam::memoryProtect, state.memoryProtect ? 127 : 0);
    out.emplace_back (TxFunctionParam::loadFunctionSelect, state.loadFunctionExt ? 127 : 0);
    return out;
}

bool looksLikeTxFunctionParamChange (const uint8_t* data, size_t size)
{
    if (data == nullptr || size != 7)
        return false;
    if (data[0] != 0xf0 || data[1] != kYamahaId)
        return false;
    if ((data[2] & 0xf0) != 0x10)
        return false;
    if (data[3] != yamahaParamGroupByte (4, 1))
        return false;
    return data[6] == 0xf7;
}

std::optional<std::pair<TxFunctionParam, uint8_t>> parseTxFunctionParamChange (const uint8_t* data,
                                                                               size_t size)
{
    if (! looksLikeTxFunctionParamChange (data, size))
        return std::nullopt;
    const auto param = static_cast<TxFunctionParam> (data[4] & 0x7f);
    switch (param)
    {
        case TxFunctionParam::dataEntryReceive:
        case TxFunctionParam::controlChangeReceive:
        case TxFunctionParam::dataEntryVolume:
        case TxFunctionParam::computeCommunication:
        case TxFunctionParam::combinedOrIndividual:
        case TxFunctionParam::noteLimitLow:
        case TxFunctionParam::noteLimitHigh:
        case TxFunctionParam::memoryProtect:
        case TxFunctionParam::loadFunctionSelect:
            return std::make_pair (param, static_cast<uint8_t> (data[5] & 0x7f));
        default:
            return std::nullopt;
    }
}

} // namespace Tx7System

namespace Tx7Performance
{

Tx7PerformanceData makeDefault()
{
    Tx7PerformanceData d {};
    applyVoiceADefaults (d);
    return d;
}

void applyVoiceADefaults (Tx7PerformanceData& d)
{
    // TX7 factory function init (service manual / owner manual init table):
    // poly, PB range 7, step 0, porta retain/time 0, MW 8+pitch, FC/AT 8, BC 15, atten 7 (loud).
    setPolyMono (d, 0); // 0 = poly
    setPitchBendRange (d, 7);
    setPitchBendStep (d, 0);
    setPortamentoTime (d, 0);
    setPortamentoGliss (d, 0); // 0 = portamento, 1 = glissando
    setPortamentoMode (d, 0);  // 0 = retain (poly) / fingered (mono)
    setModWheelSensitivity (d, 8);
    setModWheelAssign (d, kAssignPitch);
    setFootSensitivity (d, 8);
    setFootAssign (d, 0);
    setAftertouchSensitivity (d, 8);
    setAftertouchAssign (d, 0);
    setBreathSensitivity (d, 15);
    setBreathAssign (d, 0);
    setAttenuator (d, 7); // 7 = max volume (min attenuation)
}

bool looksLikePerformanceBulk (const uint8_t* data, size_t size)
{
    // F0 43 0n 01 00 5E [94] cs F7 = 6 + 94 + 2 = 102
    if (data == nullptr || size != 102)
        return false;
    if (data[0] != 0xf0 || data[1] != kYamahaId)
        return false;
    if ((data[2] & 0xf0) != 0x00)
        return false;
    if (data[3] != kFormatPerformance)
        return false;
    if (data[4] != 0x00 || data[5] != 0x5e)
        return false;
    return data[size - 1] == 0xf7;
}

bool isPerformanceBulkMessage (const uint8_t* data, size_t size)
{
    if (! looksLikePerformanceBulk (data, size))
        return false;
    const uint8_t* payload = data + 6;
    return yamahaChecksumOk (payload, static_cast<size_t> (kPerformanceDataBytes),
                             data[6 + kPerformanceDataBytes]);
}

std::optional<Tx7PerformanceData> parsePerformanceBulk (const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0)
        return std::nullopt;

    if (size == static_cast<size_t> (kPerformanceDataBytes))
    {
        Tx7PerformanceData out {};
        std::copy (data, data + kPerformanceDataBytes, out.begin());
        for (auto& b : out)
            b &= 0x7f;
        return out;
    }

    if (! isPerformanceBulkMessage (data, size))
        return std::nullopt;

    Tx7PerformanceData out {};
    std::copy (data + 6, data + 6 + kPerformanceDataBytes, out.begin());
    return out;
}

} // namespace Tx7Performance
} // namespace fmlib
