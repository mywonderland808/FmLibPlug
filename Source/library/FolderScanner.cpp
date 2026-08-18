#include "library/FolderScanner.h"
#include "sysex/FormatDetect.h"
#include "util/StringUtils.h"
#include <fstream>

namespace fmlib
{

namespace
{
bool isSyxExtension (const std::filesystem::path& p)
{
    const auto ext = asciiLower (p.extension().string());
    return ext == ".syx" || ext == ".dx7";
}
} // namespace

FolderScanner::Result FolderScanner::scan (const std::vector<std::filesystem::path>& baseFolders,
                                           std::atomic<bool>* cancelFlag,
                                           ProgressFn progress)
{
    Result result;

    for (const auto& base : baseFolders)
    {
        if (cancelFlag != nullptr && cancelFlag->load())
            break;
        if (! std::filesystem::exists (base))
            continue;

        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it (base, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment (ec))
        {
            if (cancelFlag != nullptr && cancelFlag->load())
                break;
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (! it->is_regular_file())
                continue;
            if (! isSyxExtension (it->path()))
                continue;

            ++result.filesScanned;
            std::ifstream in (it->path(), std::ios::binary);
            if (! in)
            {
                ++result.filesSkipped;
                continue;
            }
            std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
            const auto parsed = FormatDetect::parseSupported (bytes.data(), bytes.size());
            if (parsed.voices.empty())
            {
                ++result.filesSkipped;
                continue;
            }

            const auto rel = std::filesystem::relative (it->path(), base, ec);
            const auto relStr = ec ? it->path().filename().string() : rel.generic_string();

            for (const auto& v : parsed.voices)
            {
                PatchEntry e;
                e.voice = v.data;
                e.bankSlot = v.bankSlot;
                e.voiceName = voiceNameFromData (v.data);
                e.fileName = it->path().filename().string();
                e.relativePath = relStr;
                e.absolutePath = it->path();
                e.baseFolder = base;
                e.contentId = contentIdFromVoice (v.data);
                e.refreshSearchCache();
                result.entries.push_back (std::move (e));
                ++result.voicesFound;
            }

            if (progress)
                progress (result.filesScanned, result.voicesFound, result.filesSkipped);
        }
    }

    return result;
}

} // namespace fmlib
