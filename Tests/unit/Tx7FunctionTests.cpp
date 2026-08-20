#include "library/FunctionBuffer.h"
#include "sysex/FormatDetect.h"
#include "sysex/SysexMessages.h"
#include "sysex/Tx7Function.h"
#include <catch2/catch_test_macros.hpp>

using namespace fmlib;

TEST_CASE ("Tx7Performance defaults and field accessors", "[sysex][tx7][function]")
{
    auto d = Tx7Performance::makeDefault();
    REQUIRE (Tx7Performance::polyMono (d) == 0);
    REQUIRE (Tx7Performance::pitchBendRange (d) == 7);
    REQUIRE (Tx7Performance::pitchBendStep (d) == 0);
    REQUIRE (Tx7Performance::portamentoTime (d) == 0);
    REQUIRE (Tx7Performance::portamentoGliss (d) == 0);
    REQUIRE (Tx7Performance::portamentoMode (d) == 0);
    REQUIRE (Tx7Performance::modWheelSensitivity (d) == 8);
    REQUIRE (Tx7Performance::modWheelAssign (d) == Tx7Performance::kAssignPitch);
    REQUIRE (Tx7Performance::footSensitivity (d) == 8);
    REQUIRE (Tx7Performance::footAssign (d) == 0);
    REQUIRE (Tx7Performance::aftertouchSensitivity (d) == 8);
    REQUIRE (Tx7Performance::aftertouchAssign (d) == 0);
    REQUIRE (Tx7Performance::breathSensitivity (d) == 15);
    REQUIRE (Tx7Performance::breathAssign (d) == 0);
    REQUIRE (Tx7Performance::attenuator (d) == 7);

    Tx7Performance::setPolyMono (d, 1);
    Tx7Performance::setPitchBendRange (d, 12);
    Tx7Performance::setPitchBendStep (d, 1);
    Tx7Performance::setPortamentoTime (d, 50);
    Tx7Performance::setPortamentoGliss (d, 1);
    Tx7Performance::setPortamentoMode (d, 1);
    Tx7Performance::setModWheelSensitivity (d, 15);
    Tx7Performance::setModWheelAssign (d, Tx7Performance::kAssignPitch | Tx7Performance::kAssignAmp);
    Tx7Performance::setFootSensitivity (d, 10);
    Tx7Performance::setFootAssign (d, Tx7Performance::kAssignEgBias);
    Tx7Performance::setAftertouchSensitivity (d, 8);
    Tx7Performance::setBreathSensitivity (d, 4);
    Tx7Performance::setBreathAssign (d, Tx7Performance::kAssignPitch | Tx7Performance::kAssignEgBias);
    Tx7Performance::setAttenuator (d, 3);

    REQUIRE (Tx7Performance::polyMono (d) == 1);
    REQUIRE (Tx7Performance::pitchBendRange (d) == 12);
    REQUIRE (Tx7Performance::pitchBendStep (d) == 1);
    REQUIRE (Tx7Performance::portamentoTime (d) == 50);
    REQUIRE (Tx7Performance::portamentoGliss (d) == 1);
    REQUIRE (Tx7Performance::portamentoMode (d) == 1);
    REQUIRE (Tx7Performance::modWheelSensitivity (d) == 15);
    REQUIRE (Tx7Performance::modWheelAssign (d) == 3);
    REQUIRE (Tx7Performance::footSensitivity (d) == 10);
    REQUIRE (Tx7Performance::footAssign (d) == 4);
    REQUIRE (Tx7Performance::aftertouchSensitivity (d) == 8);
    Tx7Performance::setAftertouchAssign (d, Tx7Performance::kAssignPitch | Tx7Performance::kAssignAmp);
    REQUIRE (Tx7Performance::aftertouchAssign (d) == 3);
    REQUIRE (Tx7Performance::breathSensitivity (d) == 4);
    REQUIRE (Tx7Performance::breathAssign (d) == 5);
    REQUIRE (Tx7Performance::attenuator (d) == 3);
    REQUIRE (Tx7Performance::controllerSensitivityToLive (0) == 0);
    REQUIRE (Tx7Performance::controllerSensitivityToLive (5) == 33); // (5*99+7)/15
    REQUIRE (Tx7Performance::controllerSensitivityToLive (8) == 53);
    REQUIRE (Tx7Performance::controllerSensitivityToLive (15) == 99);

    Tx7Performance::setPitchBendRange (d, 99);
    REQUIRE (Tx7Performance::pitchBendRange (d) == 12);
    Tx7Performance::setPortamentoTime (d, 200);
    REQUIRE (Tx7Performance::portamentoTime (d) == 99);
    Tx7Performance::setModWheelSensitivity (d, 99);
    REQUIRE (Tx7Performance::modWheelSensitivity (d) == 15);
    Tx7Performance::setAttenuator (d, 99);
    REQUIRE (Tx7Performance::attenuator (d) == 7);

    Tx7Performance::setVoiceAField (d, Tx7Performance::kPitchBendRange, 99);
    REQUIRE (Tx7Performance::pitchBendRange (d) == 12);
    Tx7Performance::setVoiceAField (d, Tx7Performance::kAttenuator, 4);
    REQUIRE (Tx7Performance::attenuator (d) == 4);

    // Voice-A reset must not wipe a stamped mid-payload region.
    for (int i = 30; i < 40; ++i)
        Tx7Performance::set (d, i, 0x55);
    Tx7Performance::applyVoiceADefaults (d);
    REQUIRE (Tx7Performance::polyMono (d) == 0);
    REQUIRE (Tx7Performance::pitchBendRange (d) == 7);
    REQUIRE (Tx7Performance::modWheelSensitivity (d) == 8);
    REQUIRE (Tx7Performance::modWheelAssign (d) == Tx7Performance::kAssignPitch);
    REQUIRE (Tx7Performance::footSensitivity (d) == 8);
    REQUIRE (Tx7Performance::breathSensitivity (d) == 15);
    REQUIRE (Tx7Performance::aftertouchSensitivity (d) == 8);
    REQUIRE (Tx7Performance::attenuator (d) == 7);
    for (int i = 30; i < 40; ++i)
        REQUIRE (Tx7Performance::get (d, i) == 0x55);
}

TEST_CASE ("Voice-A edits preserve Voice-B bulk region", "[sysex][tx7][function]")
{
    auto d = Tx7Performance::makeDefault();
    // Stamp a synthetic Voice-B / mid-payload region (indices past Voice-A controllers).
    for (int i = 30; i < 60; ++i)
        Tx7Performance::set (d, i, static_cast<uint8_t> (0x20 + (i & 0x1f)));

    Tx7Performance::setPolyMono (d, 1);
    Tx7Performance::setModWheelSensitivity (d, 9);
    Tx7Performance::setFootAssign (d, Tx7Performance::kAssignAmp);
    Tx7Performance::setAttenuator (d, 2);

    const auto dump = SysexMessages::makePerformanceBulk (d, 1);
    const auto parsed = Tx7Performance::parsePerformanceBulk (dump);
    REQUIRE (parsed.has_value());
    REQUIRE (Tx7Performance::polyMono (*parsed) == 1);
    REQUIRE (Tx7Performance::modWheelSensitivity (*parsed) == 9);
    REQUIRE (Tx7Performance::attenuator (*parsed) == 2);
    for (int i = 30; i < 60; ++i)
        REQUIRE (Tx7Performance::get (*parsed, i) == static_cast<uint8_t> (0x20 + (i & 0x1f)));
}

TEST_CASE ("makePerformanceBulk round-trip and dump request", "[sysex][tx7][messages]")
{
    auto d = Tx7Performance::makeDefault();
    Tx7Performance::setPolyMono (d, 1);
    Tx7Performance::setPortamentoTime (d, 40);

    const auto dump = SysexMessages::makePerformanceBulk (d, 1);
    REQUIRE (dump.size() == 102);
    REQUIRE (dump[0] == 0xf0);
    REQUIRE (dump[1] == kYamahaId);
    REQUIRE (dump[3] == kFormatPerformance);
    REQUIRE (dump[4] == 0x00);
    REQUIRE (dump[5] == 0x5e);
    REQUIRE (dump.back() == 0xf7);
    REQUIRE (Tx7Performance::looksLikePerformanceBulk (dump.data(), dump.size()));
    REQUIRE (Tx7Performance::isPerformanceBulkMessage (dump.data(), dump.size()));

    const auto parsed = Tx7Performance::parsePerformanceBulk (dump);
    REQUIRE (parsed.has_value());
    REQUIRE (Tx7Performance::polyMono (*parsed) == 1);
    REQUIRE (Tx7Performance::portamentoTime (*parsed) == 40);

    const auto req = SysexMessages::makeDumpRequestFormat (kFormatPerformance, 3);
    REQUIRE (req.size() == 5);
    REQUIRE (req[2] == 0x22);
    REQUIRE (req[3] == kFormatPerformance);
}

TEST_CASE ("Performance bulk rejects bad checksum", "[sysex][tx7][messages]")
{
    auto d = Tx7Performance::makeDefault();
    auto dump = SysexMessages::makePerformanceBulk (d, 1);
    REQUIRE (Tx7Performance::looksLikePerformanceBulk (dump.data(), dump.size()));
    dump[dump.size() - 2] = static_cast<uint8_t> ((dump[dump.size() - 2] + 1) & 0x7f);
    REQUIRE (Tx7Performance::looksLikePerformanceBulk (dump.data(), dump.size()));
    REQUIRE_FALSE (Tx7Performance::isPerformanceBulkMessage (dump.data(), dump.size()));
    REQUIRE_FALSE (Tx7Performance::parsePerformanceBulk (dump).has_value());
}

TEST_CASE ("DX and TX function parameter-change group bytes", "[sysex][tx7][messages]")
{
    REQUIRE (yamahaParamGroupByte (0, 0) == 0x00);
    REQUIRE (yamahaParamGroupByte (0, 1) == 0x01);
    REQUIRE (yamahaParamGroupByte (2, 0) == 0x08);
    REQUIRE (yamahaParamGroupByte (4, 1) == 0x11);

    const auto dx = SysexMessages::makeDxFunctionParamChange (DxFunctionParam::polyMono, 1, 1);
    REQUIRE (dx.size() == 7);
    REQUIRE (dx[3] == 0x08);
    REQUIRE (dx[4] == 64);
    REQUIRE (dx[5] == 1);

    const auto tx = SysexMessages::makeTxFunctionParamChange (TxFunctionParam::memoryProtect, 0, 1);
    REQUIRE (tx[3] == 0x11);
    REQUIRE (tx[4] == 7);
    REQUIRE (tx[5] == 0);

    const auto note = SysexMessages::makeTxFunctionParamChange (TxFunctionParam::noteLimitLow, 12, 1);
    REQUIRE (note[4] == 5);
    REQUIRE (note[5] == 12);

    const auto fc = SysexMessages::makeDxFunctionParamChange (DxFunctionParam::footSensitivity, 40, 1);
    REQUIRE (fc[4] == 72);
    REQUIRE (fc[5] == 40);
}

TEST_CASE ("FunctionBuffer blocks bulk send until device Get", "[sysex][tx7][function]")
{
    FunctionBuffer buf;
    REQUIRE_FALSE (buf.canBulkSend());
    buf.ensureLocalScratch();
    REQUIRE (buf.known());
    REQUIRE_FALSE (buf.hasDeviceSnapshot());
    REQUIRE_FALSE (buf.canBulkSend());

    auto d = Tx7Performance::makeDefault();
    Tx7Performance::setPolyMono (d, 1);
    buf.setFromDevice (d);
    REQUIRE (buf.canBulkSend());
    REQUIRE_FALSE (buf.isDirty());
    REQUIRE_FALSE (buf.shouldApplyWithVoiceLoad());
    REQUIRE (Tx7Performance::polyMono (buf.getData()) == 1);

    buf.clear();
    REQUIRE_FALSE (buf.canBulkSend());
}

TEST_CASE ("FunctionBuffer apply-with-load only when dirty", "[sysex][tx7][function]")
{
    FunctionBuffer buf;
    auto d = Tx7Performance::makeDefault();
    buf.setFromDevice (d);
    REQUIRE (buf.canBulkSend());
    REQUIRE_FALSE (buf.shouldApplyWithVoiceLoad());

    buf.markDirty();
    REQUIRE (buf.shouldApplyWithVoiceLoad());

    buf.markSaved();
    REQUIRE_FALSE (buf.shouldApplyWithVoiceLoad());
}

TEST_CASE ("FormatDetect labels TX7 performance bulk", "[sysex][tx7][detect]")
{
    auto d = Tx7Performance::makeDefault();
    const auto dump = SysexMessages::makePerformanceBulk (d, 1);
    const auto det = FormatDetect::detect (dump.data(), dump.size());
    REQUIRE (det.kind == SysexFormatKind::functionDump);
    REQUIRE_FALSE (det.supported); // not a voice library format
    REQUIRE (det.label.find ("performance") != std::string::npos);

    const auto parsed = FormatDetect::parseSupported (dump.data(), dump.size());
    REQUIRE (parsed.voices.empty());
}
