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

void PatchLibrary::addListener (Listener l)
{
    listeners.push_back (std::move (l));
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
            skippedFiles = result.filesSkipped;
            entries = std::move (result.entries);
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
    for (auto& l : listeners)
        if (l)
            l();
}

} // namespace fmlib
