#pragma once

#include "sysex/Tx7Function.h"

namespace fmlib
{

/**
 * Session hold of the TX7 function / 1-performance edit buffer
 * (not part of the voice library).
 *
 * Bulk Send requires a snapshot from the device (Get Fn).
 * Apply-with-voice-load also requires dirty (edited since last Get/Send)
 * so unchanged snapshots are not re-written on every voice load.
 * Local control edits may seed a scratch buffer for live param-change only.
 */
class FunctionBuffer
{
public:
    bool known() const { return hasData; }
    /** True after a successful Get (or equivalent full dump). Safe for bulk send. */
    bool hasDeviceSnapshot() const { return fromDevice; }
    bool canBulkSend() const { return fromDevice; }
    /** Bulk send allowed and buffer differs from last Get/Send. */
    bool shouldApplyWithVoiceLoad() const { return fromDevice && dirty; }
    bool isDirty() const { return dirty; }
    void markSaved() { dirty = false; }
    void markDirty() { dirty = true; }

    void clear()
    {
        data = Tx7Performance::makeDefault();
        hasData = false;
        fromDevice = false;
        dirty = false;
    }

    /** Full dump from the TX7 — enables bulk Send; starts clean (matches device). */
    void setFromDevice (Tx7PerformanceData next)
    {
        data = std::move (next);
        hasData = true;
        fromDevice = true;
        dirty = false;
    }

    /**
     * Seed defaults for live g=2 edits before any Get.
     * Does not enable bulk send (avoids wiping the TX7 with near-empty data).
     */
    void ensureLocalScratch()
    {
        if (! hasData)
        {
            data = Tx7Performance::makeDefault();
            hasData = true;
            fromDevice = false;
        }
    }

    const Tx7PerformanceData& getData() const { return data; }
    Tx7PerformanceData& getData() { return data; }

private:
    Tx7PerformanceData data = Tx7Performance::makeDefault();
    bool hasData = false;
    bool fromDevice = false;
    bool dirty = false;
};

} // namespace fmlib
