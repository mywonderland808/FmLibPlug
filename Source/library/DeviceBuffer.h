#pragma once

#include "library/PatchEntry.h"
#include <optional>
#include <vector>

namespace fmlib
{

class DeviceBuffer
{
public:
    bool empty() const { return voices.empty(); }
    void clear() { voices.clear(); dirty = false; }

    void setVoices (std::vector<PatchEntry> v)
    {
        voices = std::move (v);
        dirty = true;
    }

    /** Ensure 32 bank slots for custom-bank editing (pads with empty INIT voices). */
    void ensureEditableBank();

    void setSlot (int index0, PatchEntry entry);
    /** Swap two voices; bankSlot values follow their new indices. */
    void swapVoices (int indexA0, int indexB0);
    PatchEntry makeEmptySlot (int bankSlot1to32) const;

    const std::vector<PatchEntry>& getVoices() const { return voices; }
    bool isDirty() const { return dirty; }
    void markSaved() { dirty = false; }
    void markDirty() { dirty = true; }

    bool isFullBank() const { return voices.size() == static_cast<size_t> (kBankVoiceCount); }

    std::optional<std::array<VoiceData, kBankVoiceCount>> asBank() const
    {
        if (! isFullBank())
            return std::nullopt;
        std::array<VoiceData, kBankVoiceCount> bank {};
        for (size_t i = 0; i < voices.size(); ++i)
            bank[i] = voices[i].voice;
        return bank;
    }

private:
    std::vector<PatchEntry> voices;
    bool dirty = false;
};

} // namespace fmlib
