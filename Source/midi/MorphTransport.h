#pragma once

#include "sysex/Dx7Formats.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace fmlib
{

/**
 * Which morph parameters are allowed on the wire while notes are sounding.
 *
 * allParams — every changed voice parameter streams immediately (full sweep).
 *             The whole voice morphs in real time; on a DX7 mkI / TX7 the
 *             envelope/level edits click, which can be musical but is audible.
 * freqOnly  — only oscillator coarse/fine/detune stream while keys are held,
 *             so pitch glides smoothly; everything else is held back and lands
 *             with the next full dump once the keyboard is quiet.
 *
 * When nothing sounds both modes behave the same: a full voice dump.
 */
enum class MorphStreamMode : int
{
    allParams = 0,
    freqOnly = 1,
};

struct MorphTransportTickResult
{
    std::vector<std::vector<uint8_t>> messages;
    bool usedFullDump = false;
    int paramsEmitted = 0;
};

/**
 * Morph SysEx planner: coalesces target updates, budgets wire traffic, and
 * respects the active stream mode (all parameters / frequency only).
 * Pure logic - MidiDeviceManager owns egress timing.
 */
class MorphTransport
{
public:
    /** Default ~6 param-change SysEx messages per tick (7 bytes each). */
    static constexpr int kDefaultByteBudget = 42;
    static constexpr int kParamChangeBytes = 7;
    static constexpr int kVoiceDumpBytes = 163;

    void setStreamMode (MorphStreamMode m) { streamMode = m; }
    void setChannel (int channel1to16);
    void setByteBudget (int bytes);
    /** True while any note is sounding (audition / controller / thru). */
    void setNotesSounding (bool sounding);

    /**
     * Update morph target. Coalesces: only the latest voice is kept.
     * forceCommit: prefer a full dump when idle (pad click / drag end / note jump).
     */
    void setTarget (const VoiceData& voice, bool forceCommit = false);

    void invalidateBaseline();
    /** Mark voice as already on the wire (after sendVoice / external dump). */
    void forceBaseline (const VoiceData& voice);
    bool hasBaseline() const { return lastSent.has_value(); }
    bool hasTarget() const { return target.has_value(); }
    int pendingDiffCount() const;

    /** Produce the next slice of SysEx to send. */
    MorphTransportTickResult tick();

    /** Osc coarse / fine / detune only (skips osc mode). */
    static bool isLiveFreqIndex (size_t voiceIndex);

    /**
     * Build one budgeted slice of param changes (and optional full dump).
     * Unit-testable without MIDI hardware.
     */
    static MorphTransportTickResult buildSlice (const VoiceData& from,
                                                const VoiceData& to,
                                                int channel,
                                                int maxBytes,
                                                bool notesSounding,
                                                MorphStreamMode streamMode,
                                                bool preferFullDump);

private:
    void applyEmittedToBaseline (const MorphTransportTickResult& slice, bool liveFreqOnly);

    MorphStreamMode streamMode = MorphStreamMode::freqOnly;
    int channel = 1;
    int byteBudget = kDefaultByteBudget;
    bool notesSounding = false;
    bool forceCommitPending = false;

    std::optional<VoiceData> target;
    std::optional<VoiceData> lastSent;
};

} // namespace fmlib
