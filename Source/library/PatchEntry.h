#pragma once

#include "sysex/Dx7Formats.h"
#include <filesystem>
#include <string>

namespace fmlib
{

struct PatchEntry
{
    std::string voiceName;
    std::string fileName;
    std::string relativePath;
    /** Lowercase caches for search (filled when scanned / assigned). */
    std::string voiceNameLower;
    std::string fileNameLower;
    std::string relativePathLower;
    std::filesystem::path absolutePath;
    std::filesystem::path baseFolder;
    int bankSlot = -1; // 1..32 or -1
    VoiceData voice {};
    uint64_t contentId = 0;
    /** Cached BrowserList::nameSortKey (filled by refreshSearchCache). */
    std::string nameSortKey;

    void refreshSearchCache();
};

} // namespace fmlib
