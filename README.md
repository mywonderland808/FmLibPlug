# FmLibPlug

**Version 1.1.0** — AGPL-3.0 JUCE 8 MIDI librarian for **Yamaha DX7 mkI / TX7** voice SysEx.

Product name: **FmLibPlug**. Release notes: [CHANGELOG.md](CHANGELOG.md). Version source: `project(FmLibPlug VERSION ...)` in `CMakeLists.txt`. Cross-platform notes: [docs/PLATFORM.md](docs/PLATFORM.md).

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
- Return toggles favorite; Space unbound (DAW transport)

Yamaha, DX7, and TX7 are trademarks of Yamaha Corporation. FmLibPlug is an independent AGPL project and is not affiliated with Yamaha.

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).

JUCE under AGPLv3. CLAP via [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions).

Do not commit third-party DX7 carts; keep local banks in gitignored folders (e.g. `Presets/`).
