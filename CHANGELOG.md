# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-08-08

### Added
- Portable `make install` with OS-default VST3/CLAP/LV2 paths (AU on macOS only)
- `scripts/install-plugins.sh` (macOS/Linux/Git Bash) and `scripts/install-plugins.ps1` (Windows)
- [docs/PLATFORM.md](docs/PLATFORM.md) — Windows/Linux build, install, and `check-hw` checklist

### Changed
- CMake requests AU/AUv3 only on Apple toolchains
- Makefile verify/force-relink find plugin binaries by layout (macOS/Linux/Windows)

### Notes
- Windows and Linux install/`check-hw` paths are implemented but **not verified on those OSes yet**
  (primary build host is macOS). Use the PLATFORM.md checklist on other devices.

## [1.0.2] - 2026-08-08

### Fixed
- Status bar “total” preset count always showed 0 after move-based filter rebuild (size read after `std::move`)
- Hide-duplicates again runs after column sort so the kept voice matches the active sort order

### Added
- Catch2 coverage for browser scope/dupe/shown counters (`BrowserList::filterForBrowser`)

## [1.0.1] - 2026-08-08

### Changed
- Morph pad throttles SysEx sends and skips pacing while dragging for smoother response
- Auto-tag runs on a background worker so large libraries no longer freeze the UI
- Build id is generated under `build/generated/` instead of rewriting `Source/BuildId.cpp`

### Removed
- Unused AuditionMidi, FunctionDump, and DuplicateDetector helpers (not part of the shipped plugin)

### Fixed
- Library filter / browser rebuilds copy less voice data on each search keystroke

## [1.0.0] - 2026-08-07

### Added
- Multi-folder `.syx` / `.dx7` library browser with background scan
- Bank / Single views, optional bank headers, prev/next bank, double-click bank load
- Direct MIDI voice and 32-voice bank load; audition note; SysEx pacing
- Device dump buffer with Get 1 / Get 32, drag-drop edit, Send (1 voice or full bank), Save
- Favorites with filled / quiet outline stars; tags (auto-tag merge + editor); search (`fav:` / `tag:` / `dupe:` / `recent:` / AND/OR)
- XY morpher with four corners and morph presets
- Dark / light theme; resizable editor; hardware checklist
- Audio passthrough insert effect
- Catch2 unit tests, Presets smoke, optional hardware MIDI harness
- `make build` / `make install` / `make plugins` with UI build-id stamp
