#pragma once

#include "library/PatchEntry.h"
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace fmlib
{

/** Per-file fingerprint for skipping unchanged .syx parses. */
struct LibraryFileFingerprint
{
    std::int64_t mtimeNs = 0;
    std::uint64_t sizeBytes = 0;

    bool operator== (const LibraryFileFingerprint& o) const
    {
        return mtimeNs == o.mtimeNs && sizeBytes == o.sizeBytes;
    }
};

struct LibraryCacheFileEntry
{
    LibraryFileFingerprint fingerprint;
    std::vector<PatchEntry> voices;
};

/**
 * On-disk cache of parsed library voices keyed by absolute path.
 * Format versioned; corrupt/unsupported files are ignored (full rescan).
 */
class LibraryCache
{
public:
    static constexpr std::uint32_t kFormatVersion = 1;

    void clear();
    bool loadFromFile (const std::filesystem::path& path);
    bool saveToFile (const std::filesystem::path& path) const;

    /** Lookup by absolute path. */
    const LibraryCacheFileEntry* find (const std::filesystem::path& absolutePath) const;
    void upsert (const std::filesystem::path& absolutePath, LibraryCacheFileEntry entry);

    /** Drop entries whose absolute path is not under any of the given base folders. */
    void retainUnderBases (const std::vector<std::filesystem::path>& baseFolders);

    static LibraryFileFingerprint fingerprintOf (const std::filesystem::path& file);

    size_t size() const { return byPath.size(); }

private:
    std::unordered_map<std::string, LibraryCacheFileEntry> byPath;
};

} // namespace fmlib
