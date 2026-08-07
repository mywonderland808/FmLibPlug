#include "library/PatchWriter.h"
#include "sysex/SysexMessages.h"
#include <fstream>

namespace fmlib
{

namespace
{
std::filesystem::path makeSyxPath (const std::filesystem::path& folder, const std::string& name)
{
    auto clean = name;
    for (auto& c : clean)
        if (c == '/' || c == '\\' || c == ':' || c == '\0')
            c = '_';
    if (clean.empty())
        clean = "untitled";
    return folder / (clean + ".syx");
}

PatchWriter::WriteResult writeBytes (const std::filesystem::path& path, const std::vector<uint8_t>& bytes, bool overwrite)
{
    PatchWriter::WriteResult r;
    r.path = path;
    if (std::filesystem::exists (path) && ! overwrite)
    {
        r.error = "exists";
        return r;
    }
    std::ofstream out (path, std::ios::binary);
    if (! out)
    {
        r.error = "cannot write";
        return r;
    }
    out.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    r.ok = static_cast<bool> (out);
    if (! r.ok)
        r.error = "write failed";
    return r;
}
} // namespace

PatchWriter::WriteResult PatchWriter::writeSingleVoice (const std::filesystem::path& folder,
                                                        const std::string& fileNameWithoutExt,
                                                        const VoiceData& voice,
                                                        int channel,
                                                        bool overwrite)
{
    const auto path = makeSyxPath (folder, fileNameWithoutExt);
    return writeBytes (path, SysexMessages::makeSingleVoiceDump (voice, channel), overwrite);
}

PatchWriter::WriteResult PatchWriter::writeBank (const std::filesystem::path& folder,
                                                 const std::string& fileNameWithoutExt,
                                                 const std::array<VoiceData, kBankVoiceCount>& voices,
                                                 int channel,
                                                 bool overwrite)
{
    const auto path = makeSyxPath (folder, fileNameWithoutExt);
    return writeBytes (path, SysexMessages::makeBankDump (voices, channel), overwrite);
}

} // namespace fmlib
