#include "sysex/Dx7Formats.h"
#include "sysex/SysexMessages.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static fmlib::VoiceData makeNamedVoice (const char* name10, uint8_t seed)
{
    fmlib::VoiceData v {};
    for (int i = 0; i < fmlib::kVoiceDataBytes; ++i)
        v[static_cast<size_t> (i)] = static_cast<uint8_t> ((seed + i) & 0x7f);
    for (int i = 0; i < fmlib::kNameLength; ++i)
        v[static_cast<size_t> (145 + i)] = static_cast<uint8_t> (name10[i] & 0x7f);
    return v;
}

static bool writeBytes (const fs::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream out (path, std::ios::binary);
    if (! out)
        return false;
    out.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    return static_cast<bool> (out);
}

int main (int argc, char** argv)
{
    const fs::path outDir = argc > 1 ? fs::path (argv[1]) : fs::path ("Tests/fixtures");
    fs::create_directories (outDir);

    const auto voice = makeNamedVoice ("TestVoice1", 3);
    auto single = fmlib::SysexMessages::makeSingleVoiceDump (voice, 1);
    if (! writeBytes (outDir / "single_163.syx", single))
    {
        std::cerr << "Failed to write single_163.syx\n";
        return 1;
    }

    auto bad = single;
    bad[6 + fmlib::kVoiceDataBytes] ^= 0x01; // flip checksum
    if (! writeBytes (outDir / "bad_checksum_163.syx", bad))
    {
        std::cerr << "Failed to write bad_checksum_163.syx\n";
        return 1;
    }

    std::array<fmlib::VoiceData, fmlib::kBankVoiceCount> bank {};
    for (int i = 0; i < fmlib::kBankVoiceCount; ++i)
    {
        char name[11] = {};
        std::snprintf (name, sizeof (name), "BankVox%02d", i + 1);
        bank[static_cast<size_t> (i)] = makeNamedVoice (name, static_cast<uint8_t> (i + 1));
    }
    auto bankDump = fmlib::SysexMessages::makeBankDump (bank, 1);
    if (! writeBytes (outDir / "bank_4104.syx", bankDump))
    {
        std::cerr << "Failed to write bank_4104.syx\n";
        return 1;
    }

    // Headerless packed bank = payload only (bytes 6 .. 6+4096 of full dump)
    std::vector<uint8_t> headerless (bankDump.begin() + 6, bankDump.begin() + 6 + fmlib::kPackedBankBytes);
    if (! writeBytes (outDir / "bank_headerless_4096.syx", headerless))
    {
        std::cerr << "Failed to write bank_headerless_4096.syx\n";
        return 1;
    }

    // Dexed-style .dx7 = raw packed bank
    if (! writeBytes (outDir / "dexed_bank.dx7", headerless))
    {
        std::cerr << "Failed to write dexed_bank.dx7\n";
        return 1;
    }

    // Minimal DX7II-like sysex (unsupported) F0 43 00 05 ... F7
    std::vector<uint8_t> dx7ii { 0xf0, 0x43, 0x00, 0x05, 0x00, 0x01, 0x00, 0xf7 };
    if (! writeBytes (outDir / "dx7ii_skip.syx", dx7ii))
    {
        std::cerr << "Failed to write dx7ii_skip.syx\n";
        return 1;
    }

    std::cout << "Wrote fixtures to " << outDir << "\n";
    return 0;
}
