#include "midi/MorphTransport.h"
#include "sysex/SysexMessages.h"
#include <algorithm>

namespace fmlib
{

namespace
{
bool isNameByte (size_t i)
{
    return i >= 145 && i < 145 + static_cast<size_t> (kNameLength);
}

bool isFreqLiveIndex (size_t i)
{
    if (i >= 126)
        return false;
    const int opLocal = static_cast<int> (i % 21);
    // Skip osc mode (17), which is discrete. Stream coarse/fine/detune only.
    return opLocal == 18 || opLocal == 19 || opLocal == 20;
}

int countWireDiffs (const VoiceData& a, const VoiceData& b)
{
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (isNameByte (i))
            continue;
        if (a[i] != b[i])
            ++n;
    }
    return n;
}
} // namespace

bool MorphTransport::isLiveFreqIndex (size_t voiceIndex)
{
    return isFreqLiveIndex (voiceIndex);
}

void MorphTransport::setChannel (int channel1to16)
{
    channel = std::clamp (channel1to16, 1, 16);
}

void MorphTransport::setByteBudget (int bytes)
{
    byteBudget = std::max (kParamChangeBytes, bytes);
}

void MorphTransport::setNotesSounding (bool sounding)
{
    notesSounding = sounding;
}

void MorphTransport::setTarget (const VoiceData& voice, bool forceCommit)
{
    target = voice;
    if (forceCommit)
        forceCommitPending = true;
}

void MorphTransport::invalidateBaseline()
{
    lastSent.reset();
    forceCommitPending = true;
}

void MorphTransport::forceBaseline (const VoiceData& voice)
{
    lastSent = voice;
    target = voice;
    forceCommitPending = false;
}

int MorphTransport::pendingDiffCount() const
{
    if (! target.has_value())
        return 0;
    if (! lastSent.has_value())
        return kVoiceDataBytes;
    return countWireDiffs (*lastSent, *target);
}

MorphTransportTickResult MorphTransport::buildSlice (const VoiceData& from,
                                                     const VoiceData& to,
                                                     int channel,
                                                     int maxBytes,
                                                     bool notesSounding,
                                                     MorphStreamMode streamMode,
                                                     bool preferFullDump)
{
    MorphTransportTickResult out;
    const int diffs = countWireDiffs (from, to);
    if (diffs == 0)
        return out;

    const bool idle = ! notesSounding;
    if (idle && (preferFullDump || diffs * kParamChangeBytes > kVoiceDumpBytes))
    {
        out.messages.push_back (SysexMessages::makeSingleVoiceDump (to, channel));
        out.usedFullDump = true;
        out.paramsEmitted = diffs;
        return out;
    }

    const bool liveFreqOnly = notesSounding && streamMode == MorphStreamMode::freqOnly;
    const int maxParams = std::max (1, maxBytes / kParamChangeBytes);
    int emitted = 0;

    for (size_t i = 0; i < to.size(); ++i)
    {
        if (isNameByte (i))
            continue;
        if (from[i] == to[i])
            continue;
        if (liveFreqOnly && ! isLiveFreqIndex (i))
            continue;

        out.messages.push_back (SysexMessages::makeParameterChange (static_cast<int> (i), to[i], channel));
        ++emitted;
        if (emitted >= maxParams)
            break;
    }

    out.paramsEmitted = emitted;
    return out;
}

void MorphTransport::applyEmittedToBaseline (const MorphTransportTickResult& slice, bool liveFreqOnly)
{
    if (! lastSent.has_value() || ! target.has_value() || slice.messages.empty())
        return;

    if (slice.usedFullDump)
    {
        lastSent = *target;
        return;
    }

    int emitted = 0;
    const int maxParams = std::max (1, byteBudget / kParamChangeBytes);
    for (size_t i = 0; i < target->size(); ++i)
    {
        if (isNameByte (i))
            continue;
        if ((*lastSent)[i] == (*target)[i])
            continue;
        if (liveFreqOnly && ! isLiveFreqIndex (i))
            continue;
        (*lastSent)[i] = (*target)[i];
        ++emitted;
        if (emitted >= maxParams)
            break;
    }
}

MorphTransportTickResult MorphTransport::tick()
{
    MorphTransportTickResult out;
    if (! target.has_value())
        return out;

    const bool idle = ! notesSounding;
    const bool liveFreqOnly = notesSounding && streamMode == MorphStreamMode::freqOnly;

    // Establish a baseline even while sounding (needed before freq-only param streaming).
    if (! lastSent.has_value())
    {
        out.messages.push_back (SysexMessages::makeSingleVoiceDump (*target, channel));
        out.usedFullDump = true;
        out.paramsEmitted = kVoiceDataBytes;
        lastSent = *target;
        forceCommitPending = false;
        return out;
    }

    const bool preferDump = forceCommitPending && idle;
    out = buildSlice (*lastSent, *target, channel, byteBudget, notesSounding, streamMode, preferDump);

    if (out.messages.empty())
    {
        if (pendingDiffCount() == 0)
            forceCommitPending = false;
        return out;
    }

    applyEmittedToBaseline (out, liveFreqOnly);
    if (out.usedFullDump || pendingDiffCount() == 0)
        forceCommitPending = false;

    return out;
}

} // namespace fmlib
