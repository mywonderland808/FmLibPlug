#pragma once

#include "library/FolderScanner.h"
#include "library/PatchEntry.h"
#include "sysex/Dx7Formats.h"
#include <juce_events/juce_events.h>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <thread>

namespace fmlib
{

/** Slot index into a PatchEntry vector (-1 = missing). Built after each library scan. */
struct BankPathIndex
{
    using SlotMap = std::array<int, kBankVoiceCount>;

    std::map<std::filesystem::path, SlotMap> byPath;

    static BankPathIndex build (const std::vector<PatchEntry>& entries);

    /** True and fills `out` only when all 32 slots are present for `absolutePath`. */
    bool completeBank (const std::vector<PatchEntry>& entries,
                       const std::filesystem::path& absolutePath,
                       std::array<VoiceData, kBankVoiceCount>& out) const;

    /** Voice row for bank path+slot (1..32). For singles use libraryIndex via PatchLibrary::findEntryByIndex. */
    std::optional<PatchEntry> findEntry (const std::vector<PatchEntry>& entries,
                                         const std::filesystem::path& absolutePath,
                                         int bankSlot) const;
};

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

    /**
     * Complete 32-voice bank for `absolutePath` without copying the whole library.
     * Empty optional if the path is missing or any slot 1..32 is absent.
     */
    std::optional<std::array<VoiceData, kBankVoiceCount>> getBankVoices (
        const std::filesystem::path& absolutePath) const;

    /** Resolve a bank-file voice by path + slot (1..32). */
    std::optional<PatchEntry> findEntry (const std::filesystem::path& absolutePath,
                                         int bankSlot) const;
    /** Resolve any voice by snapshot index (required for concatenated 1-voice files). */
    std::optional<PatchEntry> findEntryByIndex (int libraryIndex) const;

    void addListener (Listener l);
    void clearListeners() { listeners.clear(); }

    /** Test-only: replace entries and rebuild the path index (no async scan). Not for production paths. */
    void replaceEntriesForTest (std::vector<PatchEntry> next);

private:
    void handleAsyncUpdate() override;
    void applyEntriesUnlocked (std::vector<PatchEntry> next);

    std::vector<std::filesystem::path> baseFolders;
    mutable std::mutex mutex;
    std::vector<PatchEntry> entries;
    BankPathIndex bankIndex;
    std::atomic<bool> scanning { false };
    std::atomic<bool> cancel { false };
    std::atomic<int> skippedFiles { 0 };
    std::thread worker;
    std::vector<Listener> listeners;
};

} // namespace fmlib
