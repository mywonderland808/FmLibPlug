# Cross-platform build notes (FmLibPlug 1.3.x)

macOS is the primary day-to-day build host.

**What CI already covers:** GitHub Actions (`.github/workflows/ci.yml`) runs configure +
Release build + unit tests (`check`) on **macOS** and **Ubuntu**. That proves the
Linux *compile* path in CI; it does **not** replace installing plugins into a DAW
or hardware MIDI checks.

**Still checklist-only:** Windows build/install, and plugin install / host load on
Windows and Linux — run the verification checklist below before treating those as
proven.

Install toolchain and system packages first: see **Install dependencies** in
[README.md](../README.md).

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

Prefer Ninja when available (`CMAKE_BUILD_TYPE` applies):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DFMLIBPLUG_BUILD_TESTS=ON -DFMLIBPLUG_BUILD_CLAP=ON
cmake --build build -j
cmake --build build --target check
powershell -ExecutionPolicy Bypass -File scripts\install-plugins.ps1 -Config Release
cmake --build build --target check-hw-list
cmake --build build --target check-hw
```

Visual Studio 2022 (multi-config; omit `CMAKE_BUILD_TYPE`, always pass `--config`):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DFMLIBPLUG_BUILD_TESTS=ON -DFMLIBPLUG_BUILD_CLAP=ON
cmake --build build --config Release -j
cmake --build build --config Release --target check
powershell -ExecutionPolicy Bypass -File scripts\install-plugins.ps1 -Config Release
cmake --build build --config Release --target check-hw-list
cmake --build build --config Release --target check-hw
```

Git Bash + `make` also works if you prefer the Unix flow on Windows; install
defaults then follow `%CommonProgramFiles%` / `%APPDATA%` (same as PowerShell).

## Run Standalone

Standalone is **not** copied by `make install` / `install-plugins.ps1`. Launch it
from the artefact tree (use `Debug` if you configured Debug):

```bash
# macOS
open build/FmLibPlug_artefacts/Release/Standalone/FmLibPlug.app

# Linux
./build/FmLibPlug_artefacts/Release/Standalone/FmLibPlug
```

```powershell
# Windows
.\build\FmLibPlug_artefacts\Release\Standalone\FmLibPlug.exe
```

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

Use this for host install and Windows (areas CI does not cover). On Ubuntu, CI
already covers configure + build + unit tests; still run install/host items locally.

- [ ] Toolchain + system packages installed (README **Install dependencies**)
- [ ] `cmake` configure + build succeeds (Release)
- [ ] `check` (unit tests) passes
- [ ] Artefacts appear under `build/FmLibPlug_artefacts/<Config>/`
- [ ] Standalone launches from the paths above
- [ ] Install script copies VST3/CLAP/LV2; host sees FmLibPlug after full quit
- [ ] UI shows `v1.3.x` + build id
- [ ] `check-hw-list` lists expected MIDI ports
- [ ] `check-hw` dump round-trip against TX7/DX7 (optional hardware)
