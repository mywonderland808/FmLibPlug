#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace fmlib
{

enum class RtMidiKind : uint32_t
{
    shortMsg = 0,
    sysex = 1,
    sysexFragment = 2
};

struct RtMidiEvent
{
    RtMidiKind kind = RtMidiKind::shortMsg;
    int32_t sampleOffset = 0;
    uint32_t size = 0; // short: 1..3; sysex: arena byte count
    uint8_t shortData[4] {};
    double dueMs = 0.0;
};

/**
 * Lock-free SPSC MIDI event ring + SysEx byte arena.
 * Allocates only in the constructor. SysEx bytes are FIFO-ordered with events:
 * each sysex/sysexFragment event consumes the next `size` arena bytes on pop.
 */
template <int kEventSlots, int kArenaBytes>
class RtMidiRing
{
public:
    static constexpr int kMaxSysexBytes = 8192;
    using Event = RtMidiEvent;

    RtMidiRing() = default;

    uint64_t droppedEvents() const { return droppedEvents_.load (std::memory_order_relaxed); }
    uint64_t droppedSysex() const { return droppedSysex_.load (std::memory_order_relaxed); }

    void clear()
    {
        eventFifo.reset();
        arenaFifo.reset();
    }

    bool empty() const { return eventFifo.getNumReady() == 0; }
    int numReady() const { return eventFifo.getNumReady(); }

    bool tryPushShort (const uint8_t* data, int size, int sampleOffset = 0, double dueMs = 0.0)
    {
        if (data == nullptr || size < 1 || size > 3)
            return false;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        eventFifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
        {
            droppedEvents_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }

        Event& e = events[(size_t) start1];
        e.kind = RtMidiKind::shortMsg;
        e.sampleOffset = sampleOffset;
        e.size = (uint32_t) size;
        e.dueMs = dueMs;
        std::memcpy (e.shortData, data, (size_t) size);
        eventFifo.finishedWrite (1);
        return true;
    }

    bool tryPushSysex (const uint8_t* data, int size, int sampleOffset = 0, double dueMs = 0.0,
                       bool fragment = false)
    {
        if (data == nullptr || size <= 0)
            return false;
        if (size > kMaxSysexBytes || size > kArenaBytes)
        {
            droppedSysex_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }
        if (eventFifo.getFreeSpace() < 1 || arenaFifo.getFreeSpace() < size)
        {
            droppedSysex_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }

        int a1 = 0, an1 = 0, a2 = 0, an2 = 0;
        arenaFifo.prepareToWrite (size, a1, an1, a2, an2);
        if (an1 + an2 < size)
        {
            droppedSysex_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }
        if (an1 > 0)
            std::memcpy (arena.data() + (size_t) a1, data, (size_t) an1);
        if (an2 > 0)
            std::memcpy (arena.data() + (size_t) a2, data + an1, (size_t) an2);

        // Reserve the event slot before committing arena bytes. Never discardArenaBytes
        // from the producer (that advances the consumer read tip).
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        eventFifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
        {
            // Abandon the uncommitted arena prepareToWrite (no finishedWrite).
            droppedSysex_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }

        Event& e = events[(size_t) start1];
        e.kind = fragment ? RtMidiKind::sysexFragment : RtMidiKind::sysex;
        e.sampleOffset = sampleOffset;
        e.size = (uint32_t) size;
        e.dueMs = dueMs;
        arenaFifo.finishedWrite (size);
        eventFifo.finishedWrite (1);
        return true;
    }

    const Event* peek() const
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        auto& fifo = const_cast<juce::AbstractFifo&> (eventFifo);
        fifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
            return nullptr;
        return &events[(size_t) start1];
    }

    bool popCopy (Event& out, uint8_t* dest, int destCapacity)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        eventFifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
            return false;
        out = events[(size_t) start1];
        eventFifo.finishedRead (1);

        if (out.kind == RtMidiKind::shortMsg)
            return true;

        if (dest == nullptr || destCapacity < (int) out.size)
        {
            discardArenaBytes ((int) out.size);
            return false;
        }
        return readArenaBytes ((int) out.size, dest);
    }

private:
    bool readArenaBytes (int nbytes, uint8_t* dest)
    {
        int a1 = 0, an1 = 0, a2 = 0, an2 = 0;
        arenaFifo.prepareToRead (nbytes, a1, an1, a2, an2);
        if (an1 + an2 < nbytes)
            return false;
        if (an1 > 0)
            std::memcpy (dest, arena.data() + (size_t) a1, (size_t) an1);
        if (an2 > 0)
            std::memcpy (dest + an1, arena.data() + (size_t) a2, (size_t) an2);
        arenaFifo.finishedRead (nbytes);
        return true;
    }

    void discardArenaBytes (int nbytes)
    {
        int a1 = 0, an1 = 0, a2 = 0, an2 = 0;
        arenaFifo.prepareToRead (nbytes, a1, an1, a2, an2);
        const int got = an1 + an2;
        if (got > 0)
            arenaFifo.finishedRead (got);
    }

    juce::AbstractFifo eventFifo { kEventSlots };
    std::array<Event, (size_t) kEventSlots> events {};
    juce::AbstractFifo arenaFifo { kArenaBytes };
    std::array<uint8_t, (size_t) kArenaBytes> arena {};
    std::atomic<uint64_t> droppedEvents_ { 0 };
    std::atomic<uint64_t> droppedSysex_ { 0 };
};

using HostMidiRing = RtMidiRing<1024, 65536>;

template <int kSlots>
class RtShortMidiRing
{
public:
    struct Pod
    {
        uint8_t data[3] {};
        uint8_t size = 0;
        int32_t sampleOffset = 0;
    };

    void clear() { fifo.reset(); }
    bool empty() const { return fifo.getNumReady() == 0; }
    int numReady() const { return fifo.getNumReady(); }
    uint64_t dropped() const { return dropped_.load (std::memory_order_relaxed); }

    bool tryPush (const uint8_t* data, int size, int sampleOffset = 0)
    {
        if (data == nullptr || size < 1 || size > 3)
            return false;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
        {
            dropped_.fetch_add (1, std::memory_order_relaxed);
            return false;
        }
        Pod& p = slots[(size_t) start1];
        p.size = (uint8_t) size;
        p.sampleOffset = sampleOffset;
        std::memcpy (p.data, data, (size_t) size);
        fifo.finishedWrite (1);
        return true;
    }

    bool pop (Pod& out)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 + size2 < 1)
            return false;
        out = slots[(size_t) start1];
        fifo.finishedRead (1);
        return true;
    }

private:
    juce::AbstractFifo fifo { kSlots };
    std::array<Pod, (size_t) kSlots> slots {};
    std::atomic<uint64_t> dropped_ { 0 };
};

using HostShortOutRing = RtShortMidiRing<1024>;
using ThruShortRing = RtShortMidiRing<512>;

} // namespace fmlib
