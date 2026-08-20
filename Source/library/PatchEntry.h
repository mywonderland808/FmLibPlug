#pragma once

#include "sysex/Dx7Formats.h"
#include <filesystem>
#include <string>

namespace fmlib
{

/** Index metadata for a library voice (no VoiceData payload). */
struct PatchMeta
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
    uint64_t contentId = 0;
    /**
     * Index into the owning library snapshot (`PatchBrowser::all` / `PatchLibrary` entries).
     * Distinguishes concatenated 1-voice SysEx rows that share path + bankSlot -1.
     */
    int libraryIndex = -1;
    /** Cached BrowserList::nameSortKey (filled by refreshSearchCache). */
    std::string nameSortKey;

    void refreshSearchCache();
};

struct PatchEntry : PatchMeta
{
    VoiceData voice {};
};

/** True when the row came from a multi-voice bank dump (slot 1..32). */
inline bool isBankFileVoice (const PatchMeta& e) { return e.bankSlot > 0; }

/** Voice from a 1-voice SysEx (format 0x00 / Dexed 128-byte), including concatenated singles. */
inline bool isSingleVoiceFile (const PatchMeta& e) { return e.bankSlot <= 0; }

} // namespace fmlib
