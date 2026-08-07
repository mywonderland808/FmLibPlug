#include "library/PatchLibrary.h"

namespace fmlib
{

PatchLibrary::PatchLibrary() = default;

PatchLibrary::~PatchLibrary()
{
    cancel = true;
    if (worker.joinable())
        worker.join();
}

void PatchLibrary::setBaseFolders (std::vector<std::filesystem::path> folders)
{
    baseFolders = std::move (folders);
}

void PatchLibrary::rescanAsync()
{
    if (scanning.exchange (true))
        return;

    cancel = false;
    if (worker.joinable())
        worker.join();

    worker = std::thread ([this]
    {
        FolderScanner scanner;
        auto result = scanner.scan (baseFolders, &cancel);
        {
            std::lock_guard lock (mutex);
            pending = std::move (result);
        }
        scanning = false;
        triggerAsyncUpdate();
    });
}

std::vector<PatchEntry> PatchLibrary::getEntriesCopy() const
{
    std::lock_guard lock (mutex);
    return entries;
}

void PatchLibrary::handleAsyncUpdate()
{
    {
        std::lock_guard lock (mutex);
        entries = std::move (pending.entries);
        skippedFiles = pending.filesSkipped;
        pending = {};
    }
    for (auto& l : listeners)
        if (l)
            l();
}

} // namespace fmlib
