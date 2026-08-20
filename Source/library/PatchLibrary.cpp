#include "library/PatchLibrary.h"

namespace fmlib
{

namespace
{
BankPathIndex::SlotMap emptySlots()
{
    BankPathIndex::SlotMap slots {};
    slots.fill (-1);
    return slots;
}
} // namespace

BankPathIndex BankPathIndex::build (const std::vector<PatchEntry>& entries)
{
    BankPathIndex index;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (e.bankSlot < 1 || e.bankSlot > kBankVoiceCount)
            continue;
        auto& slots = index.byPath.try_emplace (e.absolutePath, emptySlots()).first->second;
        slots[static_cast<size_t> (e.bankSlot - 1)] = static_cast<int> (i);
    }
    return index;
}

bool BankPathIndex::completeBank (const std::vector<PatchEntry>& entries,
                                  const std::filesystem::path& absolutePath,
                                  std::array<VoiceData, kBankVoiceCount>& out) const
{
    const auto it = byPath.find (absolutePath);
    if (it == byPath.end())
        return false;

    for (int s = 0; s < kBankVoiceCount; ++s)
    {
        const int idx = it->second[static_cast<size_t> (s)];
        if (idx < 0 || static_cast<size_t> (idx) >= entries.size())
            return false;
        out[static_cast<size_t> (s)] = entries[static_cast<size_t> (idx)].voice;
    }
    return true;
}

std::optional<PatchEntry> BankPathIndex::findEntry (const std::vector<PatchEntry>& entries,
                                                    const std::filesystem::path& absolutePath,
                                                    int bankSlot) const
{
    if (bankSlot >= 1 && bankSlot <= kBankVoiceCount)
    {
        const auto it = byPath.find (absolutePath);
        if (it == byPath.end())
            return std::nullopt;
        const int idx = it->second[static_cast<size_t> (bankSlot - 1)];
        if (idx < 0 || static_cast<size_t> (idx) >= entries.size())
            return std::nullopt;
        return entries[static_cast<size_t> (idx)];
    }

    // Singles (bankSlot <= 0) share a path in concatenated dumps — use findEntryByIndex.
    return std::nullopt;
}

std::optional<PatchEntry> PatchLibrary::findEntryByIndex (int libraryIndex) const
{
    std::lock_guard lock (mutex);
    if (libraryIndex < 0 || static_cast<size_t> (libraryIndex) >= entries.size())
        return std::nullopt;
    return entries[static_cast<size_t> (libraryIndex)];
}

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

void PatchLibrary::applyEntriesUnlocked (std::vector<PatchEntry> next)
{
    entries = std::move (next);
    for (size_t i = 0; i < entries.size(); ++i)
        entries[i].libraryIndex = static_cast<int> (i);
    bankIndex = BankPathIndex::build (entries);
}

void PatchLibrary::replaceEntriesForTest (std::vector<PatchEntry> next)
{
    std::lock_guard lock (mutex);
    applyEntriesUnlocked (std::move (next));
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
            applyEntriesUnlocked (std::move (result.entries));
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

std::optional<std::array<VoiceData, kBankVoiceCount>> PatchLibrary::getBankVoices (
    const std::filesystem::path& absolutePath) const
{
    std::lock_guard lock (mutex);
    std::array<VoiceData, kBankVoiceCount> bank {};
    if (! bankIndex.completeBank (entries, absolutePath, bank))
        return std::nullopt;
    return bank;
}

std::optional<PatchEntry> PatchLibrary::findEntry (const std::filesystem::path& absolutePath,
                                                   int bankSlot) const
{
    std::lock_guard lock (mutex);
    return bankIndex.findEntry (entries, absolutePath, bankSlot);
}

void PatchLibrary::handleAsyncUpdate()
{
    for (auto& l : listeners)
        if (l)
            l();
}

} // namespace fmlib
