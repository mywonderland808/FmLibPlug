# FmLibPlug

**Version 1.0.1** — AGPL-3.0 JUCE 8 MIDI librarian for **Yamaha DX7 mkI / TX7** voice SysEx.

Product name: **FmLibPlug**. Release notes: [CHANGELOG.md](CHANGELOG.md). Version source: `project(FmLibPlug VERSION ...)` in `CMakeLists.txt`.

## Features

- Multi-folder recursive `.syx` / `.dx7` library (case-insensitive), background scan
- Voice names from SysEx + tooltips for file/path; optional file columns
- Bank / Single views: bank files vs one-voice files; optional bank headers (Settings); `[` / `]` prev/next bank; load bank via double-click
- Load **1 voice** (edit buffer) or **32-voice bank** via direct MIDI out; audition note
- Get / receive dumps into a temporary device buffer; drag-drop bank edit; **Send** (1 voice or full 32-bank) / **Save...**
- Favorites + search (`fav:` / `star:` / `tag:` / `dupe:` / `recent:` / AND/OR) + auto-tag (merge); click Tags to edit; Reset all tags in Settings
- XY morpher (4 corners + morph presets)
- Dark / light theme, hardware checklist in Settings; resizable editor
- SysEx pacing; audio **passthrough** insert

## Requirements

- CMake ≥ 3.22
- C++17 compiler (Xcode Clang / MSVC / GCC)
- Network on first configure (FetchContent: JUCE 8.0.14, clap-juce-extensions)

## Build

### macOS

```bash
make configure-release   # or: make configure  (Debug)
make build               # Standalone + VST3 + AU + LV2 + CLAP (+ tests)
make install             # AU/VST3/CLAP/LV2 → ~/Library/Audio/Plug-Ins/...
make plugins             # build && install
```

Artefacts:

```text
build/FmLibPlug_artefacts/Debug/   # or Release/
  Standalone/FmLibPlug.app
  VST3/FmLibPlug.vst3
  AU/FmLibPlug.component
  LV2/FmLibPlug.lv2
  CLAP/FmLibPlug.clap
```

Install destinations:

```text
~/Library/Audio/Plug-Ins/Components/FmLibPlug.component
~/Library/Audio/Plug-Ins/VST3/FmLibPlug.vst3
~/Library/Audio/Plug-Ins/CLAP/FmLibPlug.clap
~/Library/Audio/Plug-Ins/LV2/FmLibPlug.lv2
```

Standalone stays in the build tree. Quit the DAW fully after install; the UI shows version + build-id.

- `make shared` — shared code `.a` only (not host-loadable)
- AUv3: `-DFMLIBPLUG_BUILD_AUV3=ON` (often unavailable on CLI toolchains)
- Optional: `-DFMLIBPLUG_COPY_PLUGIN_AFTER_BUILD=ON`

### Windows / Linux

`make install` is macOS-only. Build with CMake; copy VST3/CLAP/LV2 into host folders. AU is Apple-only. See [docs/TODO-next.md](docs/TODO-next.md).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFMLIBPLUG_BUILD_TESTS=ON -DFMLIBPLUG_BUILD_CLAP=ON
cmake --build build --config Release -j
# artefacts: build/FmLibPlug_artefacts/<Config>/
```

| Format | Windows | Linux |
|--------|---------|-------|
| VST3 | `%CommonProgramFiles%/VST3` | `~/.vst3` |
| CLAP | `%CommonProgramFiles%/CLAP` | `~/.clap` |
| LV2 | (host-specific) | `~/.lv2` |

## Tests

Requires `FMLIBPLUG_BUILD_TESTS=ON` (default). Targets match CMake.

```bash
make check          # ctest -L unit
make check-smoke    # ctest -L smoke (needs local Presets/)
make check-all      # unit + smoke
```

Or: `cmake --build build --target check` (also `check-smoke`, `check-all`).

### Hardware MIDI (TX7 / DX7)

`make check-hw-list` — list MIDI ports. `make check-hw` — build and run `FmLibPlugHwMidiTest`. `ctest -L hw` is disabled by default.

```bash
cp Tests/hw-midi.local.json.example Tests/hw-midi.local.json
# set midiIn / midiOut from check-hw-list; "enabled": true
make check-hw
```

Config: `Tests/hw-midi.local.json` (gitignored), or `--config` / `FMLIBPLUG_HW_CONFIG`. Env overrides when set: `FMLIBPLUG_HW_MIDI`, `FMLIBPLUG_MIDI_IN` / `_OUT` / `_CH` / `_PACING_MS`, `FMLIBPLUG_HW_ALLOW_BANK_WRITE`.

- Prefer Standalone; free MIDI ports
- Computer Out → synth In; synth Out → computer In
- Channel matches local config (default 1)
- Bank overwrite: Memory Protect OFF + `allowBankWrite` / `FMLIBPLUG_HW_ALLOW_BANK_WRITE=1`
- Default suite: 1-voice dump, 32-voice dump, edit-buffer round-trip (no bank overwrite)
- Increase SysEx pacing if USB-MIDI drops data (`FMLIBPLUG_MIDI_PACING_MS` or Settings)
- Return toggles favorite; Space unbound (DAW transport)

Yamaha, DX7, and TX7 are trademarks of Yamaha Corporation. FmLibPlug is an independent AGPL project and is not affiliated with Yamaha.

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

JUCE under AGPLv3. CLAP via [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).

Do not commit third-party DX7 carts; keep local banks in gitignored folders (e.g. `Presets/`).
