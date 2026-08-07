#include "sysex/SysexMessages.h"
#include "sysex/SysexParser.h"
#include <cctype>
#include <filesystem>
#include <iostream>

int main (int argc, char** argv)
{
    namespace fs = std::filesystem;
    fs::path root = argc > 1 ? fs::path (argv[1]) : fs::path ("Presets");

    if (! fs::exists (root))
    {
        std::cerr << "Presets path not found: " << root << "\n";
        return 1;
    }

    int files = 0, ok = 0, voices = 0, failed = 0;
    constexpr int kMaxFiles = 200;

    for (auto it = fs::recursive_directory_iterator (root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it)
    {
        if (! it->is_regular_file())
            continue;
        auto ext = it->path().extension().string();
        for (auto& c : ext)
            c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
        if (ext != ".syx")
            continue;

        ++files;
        const auto parsed = fmlib::SysexParser::parseFile (it->path());
        if (parsed.voices.empty())
        {
            ++failed;
        }
        else
        {
            ++ok;
            voices += static_cast<int> (parsed.voices.size());
            // Round-trip pack/unpack checksum smoke for first voice
            const auto packed = fmlib::packVoice (parsed.voices.front().data);
            const auto again = fmlib::unpackVoice (packed);
            const auto dump = fmlib::SysexMessages::makeSingleVoiceDump (again, 1);
            if (dump.size() != 163)
                ++failed;
        }

        if (files >= kMaxFiles)
            break;
    }

    std::cout << "Scanned files: " << files << "\n"
              << "Parsed OK:     " << ok << "\n"
              << "Voices:        " << voices << "\n"
              << "Failed:        " << failed << "\n";
    return failed > files / 2 ? 2 : 0;
}
