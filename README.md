# FmLibPlug

**Version 1.3.0** — AGPL-3.0 JUCE 8 MIDI librarian for **Yamaha DX7 mkI / TX7** voice SysEx.

Product name: **FmLibPlug**. Release notes: [CHANGELOG.md](CHANGELOG.md). Version source: `project(FmLibPlug VERSION ...)` in `CMakeLists.txt`. Cross-platform notes: [docs/PLATFORM.md](docs/PLATFORM.md).

## Features

- Multi-folder recursive `.syx` / `.dx7` library (case-insensitive), background scan; Settings can enable/disable each folder
- Voice names from SysEx + tooltips for file/path; optional file columns
- Bank / All views: Bank groups by bank file; All is a flat list of every voice. Filter **`:singles`** for 1-voice SysEx (Yamaha 0x00 / Dexed 128-byte, including concatenated singles). Hide duplicates treats the same sound in another bank/slot as a dupe (name is ignored). Left/Right prev/next bank or A–Z/digit group; load bank via double-click
- Hold **Audition** (or hold `A`) to play the configured audition note; release to stop
- Context **Audition** loads the row then plays the Settings **Audition duration** note
- Right-click selects (no SysEx); left-click loads. **Set morph corner** does not pause morph
- Load **1 voice** (edit buffer) or **32-voice bank** via direct MIDI out; audition note
- Get / receive dumps into a temporary device buffer; drag-drop bank edit; **Send** (1 voice or full 32-bank) / **Save...**
- Favorites + search (`fav:` / `star:` / `tag:` / `dupe:` / `recent:` / `:singles` / AND/OR) + auto-tag (merge); click Tags to edit; Reset all tags in Settings
- XY morpher (4 corners + morph presets; parameter locks; edge LFO; Note morph via controller MIDI in)
- Morph while playing: frequency-only (default) or all-parameter streaming (see below)
- Dark / light theme, hardware checklist in Settings; resizable editor
- SysEx pacing; separate MIDI device vs controller inputs; **`(DAW)`** host MIDI bus option in Settings (plugin formats); audio **passthrough** insert

### Host integration (DAW)

Automatable parameters: **Morph X**, **Morph Y**, **Lock Ref X/Y**, **Edge LFO Rate**, **Edge LFO Sync**, **Edge LFO Div**, **Morph Motion** (Off / Random / Edges / Edge LFO). Saving the project recalls the last live morph set (corners, locks, pad). On the morph page, **Sync** locks Edge LFO to host tempo; **CCW** reverses the orbit. The selected division is one pad edge, so **1/4** is one full A-B-D-C orbit per bar of 4/4. Edge LFO keeps running while the transport is playing; Morph X/Y automation drives the pad when Edge LFO is off. In Settings, choose **`(DAW)`** on MIDI in/out/controller to route through the host instead of hardware ports (hidden in Standalone). Host MIDI into the **controller** port drives Note morph only; it is not sent to the synth unless **Forward controller MIDI to output** is on. Prefer hardware or Standalone for bulk SysEx if your DAW strips large messages on the plugin MIDI bus; avoid routing plugin MIDI out directly back into plugin MIDI in.

### Morph on DX7 mkI / TX7

Mid-note voice SysEx on an original DX7 / TX7 clicks or glitches, and that can be musical (think Microwave-style wavetable sweeps). Settings has three morph timing controls:

- **Morph while playing**
  - **Frequency only (smooth pitch)** — default. Oscillator coarse/fine/detune glide live; everything else waits for the next full dump once you stop playing.
  - **All parameters (full sweep)** — the whole voice sweeps in real time, clicks included.
- **Morph release hold (ms)** (default 250, 0–2000) — how long that saved-up dump waits after the last note-off, so it does not re-latch the patch during the release tail. Raise it for long envelopes; 0 sends immediately. A note morph, a direct pad click/drag, or a lock-reference release cancels the wait; the LFO and the automatic flush still respect it. Playing again before the hold ends restarts the timer from that release.
- **Note morph settle (ms)** (default 40, 0–100) — extra gap after the voice dump finishes on the wire before the delayed note-on. Wire time is measured automatically; raise this if note morphs still click on the attack.

Note morph jumps on the first key of a phrase and delays that note-on so the SysEx lands first. Factory locks keep **EG + Levels** at the **lock reference** (hollow marker; right-drag on the pad). Releasing Ref updates the current morph voice immediately, including while a note is held. Pad and Ref only move from clicks that start on the pad. Notes on the TX7's own keys need the controller input. **Reset defaults** restores morph, audition, UI options, SysEx pacing and controller thru (MIDI ports/channel, tags and library folders stay as they are).

## Requirements

- CMake ≥ 3.22
- C++17 compiler (Xcode Clang / MSVC / GCC)
- Network on first configure (FetchContent: JUCE 8.0.14, clap-juce-extensions)

## Build

### macOS / Linux (Make)

```bash
make configure-release   # or: make configure  (Debug)
make build               # Standalone + VST3 + LV2 + CLAP (+ AU on macOS) + tests
make install             # OS-default plugin folders (see below)
make plugins             # build && install
```

Artefacts:

```text
build/FmLibPlug_artefacts/Debug/   # or Release/
  Standalone/...
  VST3/FmLibPlug.vst3
  AU/FmLibPlug.component           # macOS only
  LV2/FmLibPlug.lv2
  CLAP/FmLibPlug.clap
```

Default install destinations (`make install`):

| OS | VST3 | CLAP | LV2 | AU |
|----|------|------|-----|----|
| macOS | `~/Library/Audio/Plug-Ins/VST3` | `…/CLAP` | `…/LV2` | `…/Components` |
| Linux | `~/.vst3` | `~/.clap` | `~/.lv2` | — |

Override with `VST3_DIR` / `CLAP_DIR` / `LV2_DIR` / `AU_DIR`. Standalone stays in the build tree. Quit the DAW fully after install; the UI shows version + build-id.

- `make shared` — shared code library only (not host-loadable)
- AUv3: `-DFMLIBPLUG_BUILD_AUV3=ON` (macOS; often unavailable on CLI toolchains)
- Optional: `-DFMLIBPLUG_COPY_PLUGIN_AFTER_BUILD=ON`

### Windows

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
  -DFMLIBPLUG_BUILD_TESTS=ON -DFMLIBPLUG_BUILD_CLAP=ON
cmake --build build --config Release -j
powershell -ExecutionPolicy Bypass -File scripts\install-plugins.ps1 -Config Release
```

Defaults: `%CommonProgramFiles%\VST3`, `%CommonProgramFiles%\CLAP`, `%APPDATA%\LV2`. AU is Apple-only.

**Status:** Windows and Linux paths are supported in scripts/docs but not yet verified on those machines — see the checklist in [docs/PLATFORM.md](docs/PLATFORM.md).

## Tests

Requires `FMLIBPLUG_BUILD_TESTS=ON` (default). Targets match CMake.

```bash
make check          # ctest -L unit
make check-smoke    # ctest -L smoke (needs local Presets/)
make check-all      # unit + smoke
```

Or: `cmake --build build --target check` (also `check-smoke`, `check-all`).

### Hardware MIDI (TX7 / DX7)

Works on macOS, Windows, and Linux via the same CMake targets (`check-hw` / `check-hw-list`).

```bash
make check-hw-list   # list MIDI ports
cp Tests/hw-midi.local.json.example Tests/hw-midi.local.json
# set midiIn / midiOut from the list; "enabled": true
make check-hw
```

Config: `Tests/hw-midi.local.json` (gitignored), or `--config` / `FMLIBPLUG_HW_CONFIG`. Env overrides when set: `FMLIBPLUG_HW_MIDI`, `FMLIBPLUG_MIDI_IN` / `_OUT` / `_CH` / `_PACING_MS`, `FMLIBPLUG_HW_ALLOW_BANK_WRITE`.

- Prefer Standalone; free MIDI ports
- Computer Out → synth In; synth Out → computer In
- Channel matches local config (default 1)
- Bank overwrite: Memory Protect OFF + `allowBankWrite` / `FMLIBPLUG_HW_ALLOW_BANK_WRITE=1`
- Default suite: 1-voice dump, 32-voice dump, edit-buffer round-trip (no bank overwrite)
- Increase SysEx pacing if USB-MIDI drops data (`FMLIBPLUG_MIDI_PACING_MS` or Settings)
- Return toggles favorite; Left/Right prev/next bank or A–Z group (when the library list has focus); hold `A` for Audition; Space unbound (DAW transport)

Yamaha, DX7, and TX7 are trademarks of Yamaha Corporation. FmLibPlug is an independent AGPL project and is not affiliated with Yamaha.

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

JUCE under AGPLv3. CLAP via [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).

Do not commit third-party DX7 carts; keep local banks in gitignored folders (e.g. `Presets/`).
