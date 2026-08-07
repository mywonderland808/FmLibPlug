#pragma once

#include "library/PatchEntry.h"
#include <filesystem>
#include <string>

namespace fmlib
{

class PatchWriter
{
public:
    struct WriteResult
    {
        bool ok = false;
        std::string error;
        std::filesystem::path path;
    };

    static WriteResult writeSingleVoice (const std::filesystem::path& folder,
                                         const std::string& fileNameWithoutExt,
                                         const VoiceData& voice,
                                         int channel = 1,
                                         bool overwrite = false);

    static WriteResult writeBank (const std::filesystem::path& folder,
                                  const std::string& fileNameWithoutExt,
                                  const std::array<VoiceData, kBankVoiceCount>& voices,
                                  int channel = 1,
                                  bool overwrite = false);
};

} // namespace fmlib
