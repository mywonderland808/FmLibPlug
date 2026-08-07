#pragma once

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
    };

    Result scan (const std::vector<std::filesystem::path>& baseFolders,
                 std::atomic<bool>* cancelFlag = nullptr,
                 ProgressFn progress = nullptr);
};

} // namespace fmlib
