#include "library/DeviceBuffer.h"
#include "sysex/Dx7Formats.h"
#include <cstring>
#include <utility>

namespace fmlib
{

PatchEntry DeviceBuffer::makeEmptySlot (int bankSlot1to32) const
{
    PatchEntry e;
    e.bankSlot = bankSlot1to32;
    e.voiceName = "(empty)";
    e.fileName = "device";
    e.relativePath = "Device";
    e.voice = {};
    // DX7 INIT-ish name in VCED bytes 145..154
    static const char name[11] = "(empty)   ";
    for (int i = 0; i < kNameLength; ++i)
        e.voice[static_cast<size_t> (145 + i)] = static_cast<uint8_t> (name[i]);
    e.contentId = 0;
    e.refreshSearchCache();
    return e;
}

void DeviceBuffer::ensureEditableBank()
{
    if (voices.size() == static_cast<size_t> (kBankVoiceCount))
        return;

    std::vector<PatchEntry> next;
    next.reserve (static_cast<size_t> (kBankVoiceCount));
    for (int i = 0; i < kBankVoiceCount; ++i)
    {
        if (i < static_cast<int> (voices.size()))
        {
            auto e = voices[static_cast<size_t> (i)];
            e.bankSlot = i + 1;
            next.push_back (std::move (e));
        }
        else
        {
            next.push_back (makeEmptySlot (i + 1));
        }
    }
    voices = std::move (next);
    dirty = true;
}

void DeviceBuffer::setSlot (int index0, PatchEntry entry)
{
    ensureEditableBank();
    if (index0 < 0 || index0 >= kBankVoiceCount)
        return;
    entry.bankSlot = index0 + 1;
    if (entry.voiceName.empty())
        entry.voiceName = voiceNameFromData (entry.voice);
    entry.refreshSearchCache();
    voices[static_cast<size_t> (index0)] = std::move (entry);
    dirty = true;
}

void DeviceBuffer::swapVoices (int indexA0, int indexB0)
{
    const int n = static_cast<int> (voices.size());
    if (indexA0 < 0 || indexB0 < 0 || indexA0 >= n || indexB0 >= n || indexA0 == indexB0)
        return;

    std::swap (voices[static_cast<size_t> (indexA0)], voices[static_cast<size_t> (indexB0)]);
    voices[static_cast<size_t> (indexA0)].bankSlot = indexA0 + 1;
    voices[static_cast<size_t> (indexB0)].bankSlot = indexB0 + 1;
    dirty = true;
}

} // namespace fmlib
