#pragma once

#include "library/FolderScanner.h"
#include "library/PatchEntry.h"
#include <juce_events/juce_events.h>
#include <functional>
#include <thread>

namespace fmlib
{

class PatchLibrary : private juce::AsyncUpdater
{
public:
    using Listener = std::function<void()>;

    PatchLibrary();
    ~PatchLibrary() override;

    void setBaseFolders (std::vector<std::filesystem::path> folders);
    const std::vector<std::filesystem::path>& getBaseFolders() const { return baseFolders; }

    void rescanAsync();
    bool isScanning() const { return scanning.load(); }

    std::vector<PatchEntry> getEntriesCopy() const;
    int getSkippedFileCount() const { return skippedFiles.load(); }

    void addListener (Listener l);
    void clearListeners() { listeners.clear(); }

private:
    void handleAsyncUpdate() override;

    std::vector<std::filesystem::path> baseFolders;
    mutable std::mutex mutex;
    std::vector<PatchEntry> entries;
    std::atomic<bool> scanning { false };
    std::atomic<bool> cancel { false };
    std::atomic<int> skippedFiles { 0 };
    std::thread worker;
    std::vector<Listener> listeners;
};

} // namespace fmlib
