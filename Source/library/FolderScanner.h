#pragma once

#include "library/LibraryCache.h"
#include "library/PatchEntry.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace fmlib
{

class FolderScanner
{
public:
    using ProgressFn = std::function<void(int filesSeen, int voicesFound, int skipped)>;

    struct Result
    {
        std::vector<PatchEntry> entries;
        int filesScanned = 0;
        int filesSkipped = 0;
        int voicesFound = 0;
        int filesFromCache = 0;
    };

    /**
     * Scan base folders for .syx/.dx7.
     * If `cachePath` is non-empty, load/save a persistent parse cache (mtime+size fingerprints).
     */
    Result scan (const std::vector<std::filesystem::path>& baseFolders,
                 std::atomic<bool>* cancelFlag = nullptr,
                 ProgressFn progress = nullptr,
                 const std::filesystem::path& cachePath = {});
};

} // namespace fmlib
