# Threading and real-time safety

FmLibPlug is a MIDI/SysEx librarian: audio is passthrough. The real-time boundary is the **audio thread** in `(DAW)` host-MIDI mode.

## Threads

| Thread | Role |
|--------|------|
| Audio (`processBlock`) | Host MIDI in/out exchange; morph parameter chase; thru scratch |
| Message / UI | Ring pumps, SysEx handlers, settings, editor |
| JUCE MIDI input callback | Hardware device / controller input (not host bus) |
| Thru-flush worker | Hardware controller-thru POD ring → `MidiOutput` |
| Morph / host pump timer | `MidiDeviceManager` timer: drain InRing + morph transport ticks |
| Library scan / cache | Background folder scan (never touches audio) |

## Audio-thread rules

On the audio thread **do not**:

- Allocate (heap / `juce::String` / growing containers)
- Take locks shared with the message thread
- Call `MessageManager::callAsync` / post messages
- Start threads
- Log or do file I/O

## Data flows

```
Host MIDI in ──(audio)──► InRing (SPSC + SysEx arena)
                              │
                              ▼  timer / pumpHostMidiInput()
                         handleController / handleDeviceSysex

Message/backend ──► OutShort (notes/CC) ──┐
                  ──► OutSysex (paced)  ──┼──► collectHostMidiOutput (audio)
Same-block thru scratch (audio) ──────────┘

Hardware thru ──► ThruPod AbstractFifo ──► thru worker ──► MidiOutput
```

### Ordering

- **OutShort** and same-block thru may **overtake** paced SysEx still waiting in OutSysex (intentional: notes stay live during morph/bulk).
- Within OutSysex, due-time is head-of-line: the first not-due event stops SysEx collect for that block.
- Due **small** SysEx (param changes, etc.) may all drain in one block; at most **one large** SysEx (≥ 256 bytes, e.g. voice/bank dump) per audio block (host/LV2-friendly).
- Morph pending flags drain on the same `MidiDeviceManager` message-thread timer as InRing — never `startTimer` / `callAsync` from the audio thread.
- Host out rings are cleared only after `hostOutputActive` is cleared and `audioHostMidiDepth` is zero (port change / close).

## Capacities and overflow

| Buffer | Capacity | Overflow |
|--------|----------|----------|
| InRing events | 1024 | Drop short event; clear held bit on note-off |
| InRing / OutSysex arena | 64 KiB | Drop SysEx; status “Host SysEx dropped — use hardware port” |
| Max SysEx | 8192 bytes | Drop |
| OutShort | 1024 | Drop (count) |
| Host thru scratch | 256 × 3 bytes | Drop; clear held on note-off |
| Hardware thru | 512 POD | Drop; clear held on note-off |

Producer-side SysEx enqueue returns `false` on overflow (bulk/morph callers can retry). Never silently drop scheduled SysEx without a status.

## Ownership

| Data | Writer | Reader |
|------|--------|--------|
| InRing | Audio | Message (pump) |
| OutShort / OutSysex | Message (+ producer mutex) | Audio (only while atomic `hostOutput`) |
| Host thru scratch | Audio (same `exchangeHostMidi`) | Audio (collect) |
| `morphEmitMsAtomic` | Message (UI / prefs) | Audio (`requestApplyLiveMorph`) |
| Morph apply / UI sync flags | Audio (atomics) | Message (`messagePumpFn`) |
| Thru held-note bits | Audio / thru | Morph sounding query |
| APVTS / live morph pad | Message / audio (atomics + pending flags) | Message (apply) |

## Lifecycle

1. Open `(DAW)` in/controller → start host pump timer (when a message manager exists).
2. Open hardware out → start thru-flush worker (**not** from the audio thread).
3. `close()` / destructor: stop timer, join thru worker, clear rings, `alive` flag for async callbacks.
4. Host port clear (`clearHostRingsSafe`) blocks the **message thread** until `audioHostMidiDepth` reaches 0 (audio has left `exchangeHostMidi`). If a host freezes the audio callback mid-exchange, the UI can stall; after ~3s a status line and debug assert fire, but the wait continues (unsafe to `AbstractFifo::reset` while audio still touches the rings).

## Verification

```bash
cmake --build build --target FmLibPlugUnitTests
./build/FmLibPlugUnitTests "[midi]"
./build/FmLibPlugUnitTests "[rt]"
```

- Alloc guard: `Tests/unit/AllocGuard.h` + `RealtimeSafetyTests.cpp` (test binary only).
- Ring stress: `RtMidiRingTests.cpp`.
- Optional: ThreadSanitizer / Clang RealtimeSanitizer builds (see CI).

## Format notes

- Prefer **hardware** ports for large bulks; many hosts truncate SysEx on the plugin MIDI bus.
- The host owns the `MidiBuffer` passed to `processBlock`. `collectHostMidiOutput` calls `MidiBuffer::addEvent`, which may allocate if the wrapper buffer is undersized (e.g. first ~4 KiB bank into a ~2 KiB VST3 buffer). We do **not** `ensureSize` on the audio thread. Unit tests that assert zero allocations warm capacity first; cold host buffers are not claimed RT-safe for huge SysEx.
- VST3/AU/LV2/CLAP differ in SysEx packaging; JUCE maps where possible. CLAP SysEx pointers are only valid for the process call — collect copies into the host `MidiBuffer` before returning.
