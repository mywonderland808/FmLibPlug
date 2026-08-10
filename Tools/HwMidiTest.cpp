#include "midi/MidiDeviceManager.h"
#include "sysex/Dx7Formats.h"
#include "sysex/SysexMessages.h"
#include "sysex/SysexParser.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
struct ReceivedDump
{
    std::mutex mutex;
    std::vector<std::vector<uint8_t>> messages;
};

struct HwConfig
{
    bool enabled = false;
    juce::String midiIn;
    juce::String midiOut;
    int channel = 1;
    int pacingMs = 20;
    bool allowBankWrite = false;
    juce::File loadedFrom;
};

fmlib::VoiceData makeTestVoice (uint8_t seed)
{
    fmlib::PackedVoice packed {};
    for (int i = 0; i < fmlib::kPackedVoiceBytes; ++i)
        packed[static_cast<size_t> (i)] = static_cast<uint8_t> ((seed + i) & 0x7f);
    const char* name = "HwTestVox1";
    for (int i = 0; i < fmlib::kNameLength; ++i)
        packed[static_cast<size_t> (118 + i)] = static_cast<uint8_t> (name[i]);
    return fmlib::unpackVoice (packed);
}

std::array<fmlib::VoiceData, fmlib::kBankVoiceCount> makeTestBank()
{
    std::array<fmlib::VoiceData, fmlib::kBankVoiceCount> bank {};
    for (int i = 0; i < fmlib::kBankVoiceCount; ++i)
    {
        fmlib::PackedVoice packed {};
        for (int b = 0; b < fmlib::kPackedVoiceBytes; ++b)
            packed[static_cast<size_t> (b)] = static_cast<uint8_t> ((i + 1 + b) & 0x7f);
        char name[11] = {};
        std::snprintf (name, sizeof (name), "HwBank_%02d", i + 1);
        for (int n = 0; n < fmlib::kNameLength; ++n)
            packed[static_cast<size_t> (118 + n)] = static_cast<uint8_t> (name[n]);
        bank[static_cast<size_t> (i)] = fmlib::unpackVoice (packed);
    }
    return bank;
}

bool waitForVoices (ReceivedDump& dump, size_t minVoices, int timeoutMs, fmlib::ParseResult& out)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::lock_guard<std::mutex> lock (dump.mutex);
            for (const auto& msg : dump.messages)
            {
                auto parsed = fmlib::SysexParser::parseBytes (msg);
                if (parsed.voices.size() >= minVoices)
                {
                    out = std::move (parsed);
                    return true;
                }
            }
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }
    return false;
}

void clearDump (ReceivedDump& dump)
{
    std::lock_guard<std::mutex> lock (dump.mutex);
    dump.messages.clear();
}

std::optional<juce::String> envString (const char* key)
{
    if (const char* v = std::getenv (key))
        if (v[0] != '\0')
            return juce::String (v);
    return std::nullopt;
}

std::optional<bool> envBool01 (const char* key)
{
    const auto s = envString (key);
    if (! s.has_value())
        return std::nullopt;
    if (*s == "1" || s->equalsIgnoreCase ("true") || s->equalsIgnoreCase ("yes"))
        return true;
    if (*s == "0" || s->equalsIgnoreCase ("false") || s->equalsIgnoreCase ("no"))
        return false;
    return std::nullopt;
}

std::vector<juce::File> candidateConfigFiles (const juce::String& cliPath)
{
    std::vector<juce::File> out;
    if (cliPath.isNotEmpty())
        out.push_back (juce::File (cliPath));
    if (auto p = envString ("FMLIBPLUG_HW_CONFIG"))
        out.push_back (juce::File (*p));

#ifdef FMLIBPLUG_SOURCE_DIR
    out.push_back (juce::File (FMLIBPLUG_SOURCE_DIR).getChildFile ("Tests/hw-midi.local.json"));
#endif
    out.push_back (juce::File::getCurrentWorkingDirectory().getChildFile ("Tests/hw-midi.local.json"));
    out.push_back (juce::File::getCurrentWorkingDirectory().getChildFile ("hw-midi.local.json"));
    return out;
}

std::optional<HwConfig> loadConfigFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    const auto text = file.loadFileAsString();
    const auto parsed = juce::JSON::parse (text);
    if (! parsed.isObject())
    {
        std::cerr << "WARN: ignoring invalid JSON in " << file.getFullPathName() << "\n";
        return std::nullopt;
    }

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    HwConfig cfg;
    cfg.loadedFrom = file;
    cfg.enabled = (bool) obj->getProperty ("enabled");
    cfg.midiIn = obj->getProperty ("midiIn").toString();
    cfg.midiOut = obj->getProperty ("midiOut").toString();
    if (obj->hasProperty ("channel"))
        cfg.channel = juce::jlimit (1, 16, (int) obj->getProperty ("channel"));
    if (obj->hasProperty ("pacingMs"))
        cfg.pacingMs = juce::jmax (0, (int) obj->getProperty ("pacingMs"));
    cfg.allowBankWrite = (bool) obj->getProperty ("allowBankWrite");
    return cfg;
}

HwConfig resolveConfig (const juce::String& cliConfigPath)
{
    HwConfig cfg;
    for (const auto& candidate : candidateConfigFiles (cliConfigPath))
    {
        if (auto loaded = loadConfigFile (candidate))
        {
            cfg = *loaded;
            std::cout << "Loaded MIDI config: " << candidate.getFullPathName() << "\n";
            break;
        }
    }

    // Environment overrides file (and fills gaps)
    if (auto v = envBool01 ("FMLIBPLUG_HW_MIDI"))
        cfg.enabled = *v;
    if (auto v = envString ("FMLIBPLUG_MIDI_IN"))
        cfg.midiIn = *v;
    if (auto v = envString ("FMLIBPLUG_MIDI_OUT"))
        cfg.midiOut = *v;
    if (auto v = envString ("FMLIBPLUG_MIDI_CH"))
        cfg.channel = juce::jlimit (1, 16, v->getIntValue());
    if (auto v = envString ("FMLIBPLUG_MIDI_PACING_MS"))
        cfg.pacingMs = juce::jmax (0, v->getIntValue());
    if (auto v = envBool01 ("FMLIBPLUG_HW_ALLOW_BANK_WRITE"))
        cfg.allowBankWrite = *v;

    return cfg;
}

void printDevices (fmlib::MidiDeviceManager& midi)
{
    std::cout << "MIDI inputs:\n";
    for (const auto& n : midi.getInputNames())
        std::cout << "  - " << n << "\n";
    std::cout << "MIDI outputs:\n";
    for (const auto& n : midi.getOutputNames())
        std::cout << "  - " << n << "\n";
}

int fail (const std::string& msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String cliConfigPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg (argv[i]);
        if (arg == "--list-devices")
        {
            fmlib::MidiDeviceManager midi;
            printDevices (midi);
            return 0;
        }
        if (arg == "--config" && i + 1 < argc)
            cliConfigPath = argv[++i];
    }

    const auto cfg = resolveConfig (cliConfigPath);

    if (! cfg.enabled)
    {
        std::cout << "Hardware MIDI tests skipped.\n";
        std::cout << "Enable via Tests/hw-midi.local.json (copy from .example) with \"enabled\": true,\n";
        std::cout << "or set FMLIBPLUG_HW_MIDI=1. List ports: FmLibPlugHwMidiTest --list-devices\n";
        return 0;
    }

    if (cfg.midiIn.isEmpty() || cfg.midiOut.isEmpty())
        return fail ("midiIn/midiOut missing — set them in Tests/hw-midi.local.json or FMLIBPLUG_MIDI_IN/OUT");

    fmlib::MidiDeviceManager midi;
    ReceivedDump dump;

    midi.setStatusCallback ([] (const juce::String& s) { std::cout << "[midi] " << s << "\n"; });
    midi.setSysexReceivedCallback ([&dump] (const std::vector<uint8_t>& bytes)
    {
        std::lock_guard<std::mutex> lock (dump.mutex);
        dump.messages.push_back (bytes);
        std::cout << "[midi] received SysEx " << bytes.size() << " bytes\n";
    });

    midi.refreshDevices();
    printDevices (midi);

    if (! midi.openInputByName (cfg.midiIn))
        return fail ("Could not open MIDI input: " + cfg.midiIn.toStdString());
    if (! midi.openOutputByName (cfg.midiOut))
        return fail ("Could not open MIDI output: " + cfg.midiOut.toStdString());

    midi.setChannel (cfg.channel);
    midi.setSysexPacingMs (cfg.pacingMs);
    std::cout << "Channel " << midi.getChannel() << ", pacing " << midi.getSysexPacingMs() << " ms\n";

    fmlib::ParseResult parsed;

    clearDump (dump);
    if (! midi.requestDump (false))
        return fail ("requestDump(voice) send failed");
    if (! waitForVoices (dump, 1, 5000, parsed))
        return fail ("Timed out waiting for 1-voice dump (check cabling, channel, Memory Protect N/A)");
    std::cout << "OK: 1-voice dump (" << parsed.voices.size() << " voice)\n";

    clearDump (dump);
    if (! midi.requestDump (true))
        return fail ("requestDump(bank) send failed");
    if (! waitForVoices (dump, 32, 15000, parsed))
        return fail ("Timed out waiting for 32-voice dump");
    std::cout << "OK: 32-voice dump (" << parsed.voices.size() << " voices)\n";

    const auto sentVoice = makeTestVoice (42);
    const auto sentId = fmlib::contentIdFromVoice (sentVoice);
    clearDump (dump);
    if (! midi.sendVoice (sentVoice))
        return fail ("sendVoice failed");
    std::this_thread::sleep_for (std::chrono::milliseconds (200));
    if (! midi.requestDump (false))
        return fail ("requestDump after sendVoice failed");
    if (! waitForVoices (dump, 1, 5000, parsed))
        return fail ("Timed out on edit-buffer round-trip dump");
    if (parsed.voices.empty() || fmlib::contentIdFromVoice (parsed.voices.front().data) != sentId)
        return fail ("Edit-buffer round-trip mismatch (contentId)");
    std::cout << "OK: edit-buffer voice round-trip\n";

    const int wrongCh = cfg.channel == 16 ? 1 : cfg.channel + 1;
    midi.setChannel (wrongCh);
    clearDump (dump);
    midi.requestDump (false);
    const bool gotUnexpected = waitForVoices (dump, 1, 1500, parsed);
    midi.setChannel (cfg.channel);
    if (gotUnexpected)
        std::cout << "WARN: received dump on wrong channel " << wrongCh
                  << " (device may ignore channel or omni) — continuing\n";
    else
        std::cout << "OK: no dump on wrong channel " << wrongCh << "\n";

    if (cfg.allowBankWrite)
    {
        std::cout << "WARNING: allowBankWrite — overwriting TX7/DX7 32-voice memory.\n";
        std::cout << "Ensure Memory Protect is OFF.\n";
        const auto bank = makeTestBank();
        clearDump (dump);
        if (! midi.sendBank (bank))
            return fail ("sendBank failed");
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        if (! midi.requestDump (true))
            return fail ("requestDump after sendBank failed");
        if (! waitForVoices (dump, 32, 15000, parsed))
            return fail ("Timed out on bank round-trip dump");
        if (parsed.voices.size() != 32)
            return fail ("Bank round-trip voice count mismatch");
        for (int i = 0; i < fmlib::kBankVoiceCount; ++i)
        {
            if (fmlib::contentIdFromVoice (parsed.voices[static_cast<size_t> (i)].data)
                != fmlib::contentIdFromVoice (bank[static_cast<size_t> (i)]))
                return fail ("Bank round-trip mismatch at slot " + std::to_string (i + 1));
        }
        std::cout << "OK: bank round-trip\n";
    }
    else
    {
        std::cout << "SKIP: bank overwrite (set allowBankWrite in local config or FMLIBPLUG_HW_ALLOW_BANK_WRITE=1)\n";
    }

    // Opt-in morph pacing probe: hold a note, stream coarse-freq param changes, verify edit buffer.
    if (envBool01 ("FMLIBPLUG_HW_MORPH_PROBE").value_or (false))
    {
        std::cout << "Morph probe: sending paced coarse-frequency param changes while a note sounds...\n";
        auto voice = makeTestVoice (7);
        voice[18] = 1;
        if (! midi.sendVoice (voice))
            return fail ("morph probe sendVoice failed");
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        midi.sendNoteOn (60, 100);
        const int spacings[] = { 20, 10, 5, 3 };
        int best = -1;
        for (int spacing : spacings)
        {
            for (int step = 0; step < 8; ++step)
            {
                const auto msg = fmlib::SysexMessages::makeParameterChange (18, static_cast<uint8_t> (2 + step), cfg.channel);
                if (! midi.sendRaw (msg, false))
                    return fail ("morph probe param send failed");
                std::this_thread::sleep_for (std::chrono::milliseconds (spacing));
            }
            clearDump (dump);
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
            midi.sendNoteOff (60);
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
            if (! midi.requestDump (false))
                return fail ("morph probe dump request failed");
            if (! waitForVoices (dump, 1, 5000, parsed))
                return fail ("morph probe dump timeout");
            if (parsed.voices.empty() || parsed.voices.front().data[18] != 9)
            {
                std::cout << "WARN: morph probe lost updates at spacing " << spacing << " ms\n";
                break;
            }
            best = spacing;
            std::cout << "OK: morph probe held at spacing " << spacing << " ms\n";
            midi.sendNoteOn (60, 100);
            voice[18] = 1;
            midi.sendVoice (voice);
            std::this_thread::sleep_for (std::chrono::milliseconds (80));
        }
        midi.sendNoteOff (60);
        if (best < 0)
            std::cout << "WARN: morph probe did not find a safe spacing\n";
        else
            std::cout << "Morph probe fastest safe spacing: " << best << " ms\n";
    }
    else
    {
        std::cout << "SKIP: morph probe (set FMLIBPLUG_HW_MORPH_PROBE=1)\n";
    }

    midi.close();
    std::cout << "All enabled hardware tests passed.\n";
    return 0;
}
