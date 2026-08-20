#include "sysex/Tx7Function.h"

namespace fmlib
{
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
