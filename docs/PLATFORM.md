# Cross-platform build notes (FmLibPlug 1.1+)

macOS is the primary day-to-day build host. **Windows and Linux build/install
paths are implemented but not verified on those OSes yet** — run the checklist
below on a Windows PC and a Linux box before treating 1.1 as proven there.

## Formats

| Format | macOS | Windows | Linux |
|--------|-------|---------|-------|
| Standalone | yes (`.app`) | yes (`.exe`) | yes |
| VST3 | yes | yes | yes |
| CLAP | yes | yes | yes |
| LV2 | yes | yes | yes |
| AU / AUv3 | yes | — | — |

## Quick build

### macOS / Linux (Make)

```bash
make configure-release   # or: make configure
make build
make install             # OS-default plugin folders
# or: make plugins
make check
make check-hw-list       # MIDI ports
make check-hw            # needs Tests/hw-midi.local.json
```

### Windows (CMake + PowerShell)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
  -DFMLIBPLUG_BUILD_TESTS=ON -DFMLIBPLUG_BUILD_CLAP=ON
cmake --build build --config Release -j
cmake --build build --config Release --target check
powershell -ExecutionPolicy Bypass -File scripts\install-plugins.ps1 -Config Release
cmake --build build --config Release --target check-hw-list
cmake --build build --config Release --target check-hw
```

Git Bash + `make` also works if you prefer the Unix flow on Windows; install
defaults then follow `%CommonProgramFiles%` / `%APPDATA%` (same as PowerShell).

## Default install locations

| OS | VST3 | CLAP | LV2 | AU |
|----|------|------|-----|----|
| macOS | `~/Library/Audio/Plug-Ins/VST3` | `…/CLAP` | `…/LV2` | `…/Components` |
| Linux | `~/.vst3` | `~/.clap` | `~/.lv2` | — |
| Windows | `%CommonProgramFiles%\VST3` | `%CommonProgramFiles%\CLAP` | `%APPDATA%\LV2` | — |

Override with `VST3_DIR` / `CLAP_DIR` / `LV2_DIR` / `AU_DIR` (Make/bash) or
`-Vst3Dir` / `-ClapDir` / `-Lv2Dir` (PowerShell).

## Hardware MIDI checklist (all platforms)

1. `make check-hw-list` or CMake target `check-hw-list`
2. Copy `Tests/hw-midi.local.json.example` → `Tests/hw-midi.local.json`
3. Set exact `midiIn` / `midiOut` names from the list; `"enabled": true`
4. Prefer Standalone so the DAW is not holding the ports
5. `make check-hw` (or CMake `check-hw`)

Bank overwrite only with Memory Protect OFF and `allowBankWrite` /
`FMLIBPLUG_HW_ALLOW_BANK_WRITE=1`.

## Verification checklist (Windows / Linux owners)

- [ ] `cmake` configure + build succeeds (Release)
- [ ] `check` (unit tests) passes
- [ ] Artefacts appear under `build/FmLibPlug_artefacts/<Config>/`
- [ ] Install script copies VST3/CLAP/LV2; host sees FmLibPlug after full quit
- [ ] UI shows `v1.1.x` + build id
- [ ] `check-hw-list` lists expected MIDI ports
- [ ] `check-hw` dump round-trip against TX7/DX7 (optional hardware)
