# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Settings **List button shows**: All voices (flat, including single-voice SysEx) or Single-voice SysEx only; Bank view always groups by bank file name
- Library / device buffer context menu: **Set morph corner** (A–D) (does not load or pause morph) and **Audition** (loads, then timed note)
- Audition button is press-and-hold (note on down, off on release); hold `A` mirrors the button press look
- Settings library folders: per-folder enable checkbox (Rescan applies the set)
- All / Single views: `< A–Z` / `A–Z >` (Left/Right) jump by first letter or digit of the voice name

### Changed
- Morph stays active on Library / Morph Presets; ends on device-buffer load or Library-page voice load; Morph-page library click turns off Edge LFO and Note morph and pauses egress until the pad is used again or Edge LFO / Note morph is turned back on
- Pad, Edge LFO and Note morph stay visible but disabled until all four corners are set
- Morph pad corner labels always include A–D (`A: BRASS`, `A: (unset)`)
- Edge LFO and Note morph are mutually exclusive
- Lock-reference release updates the voice immediately at the main marker, including locked EG/levels while a note is held
- Settings **Audition duration (ms)** applies to context-menu Audition only (header Audition is hold-to-play)
- Bank navigation shortcuts are Left/Right (was `P` / `N`); hold `A` for Audition
- Audition keeps sounding while either the button or `A` remains down

### Fixed
- Turning Edge LFO or Note morph back on resumes morph after a Morph-page library click (no pad click required)
- Pad and lock-ref markers only move from clicks that start on the morph pad
- Lock-reference release streams locked-group changes while a note is held (Frequency-only mode no longer waits for silence)
- Lock chips and Reset stream locked-group changes while a note is held
- Frequency-only resumes after a lock-ref flush even if Edge LFO is still moving pitch
- Clicks outside the morph pad cancel a stuck pad / lock-ref drag
- Left-click on the pad does not move the marker while Edge LFO is running (right-drag lock-ref still works)
- Note morph hint updates when Edge LFO is turned off by enabling Note morph
- Left/Right are not consumed in All/Single unless the list is sorted by Patch name
- Editor teardown stops held audition notes
- Right-click on library or device buffer selects without SysEx (popup is detected on mouse-down, before the menu handler); context **Audition** loads then plays the note; **Set morph corner** does not pause morph
- Right-click on morph presets selects and loads like a left-click
- Settings sliders no longer rewrite preferences on every drag tick
- Prefs persist writes settings.xml only; tags and morph presets save when those stores change (library clicks no longer rewrite all three files)
- All/Single A-Z jump is enabled only when the list is sorted by Patch name (ASCII labels)
- Patch-name sort and A-Z jumps: letter groups A-Z (lowercase, leading junk skipped); numbers and symbols share one group
- Linux `make build`: `force-relink-clean` no longer deletes VST3/LV2 bundle directories without recreating them, which made `ld` fail (`No such file or directory`)
- Linux/Windows CLAP verification: `fmlib_find_binary` treats a single `FmLibPlug.clap` file as the plugin binary (not only macOS bundles)

## [1.2.0] - 2026-08-11

### Added
- Settings **Morph while playing**: *Frequency only (smooth pitch)* (default) streams osc coarse/fine/detune while keys are held and holds the rest for silence; *All parameters (full sweep)* streams every change live
- Settings **Morph release hold (ms)** (0-2000, default 250): delay morph SysEx after the last note-off so a voice dump does not re-latch during the release tail; pad drag and note morph cancel the wait
- Settings **Note morph settle (ms)** (0-100, default 40): gap after a morph voice dump finishes on the wire before the delayed note-on
- Settings **Reset defaults** restores morph, audition, UI options, SysEx pacing and controller thru (MIDI ports/channel, tags and library folders unchanged); sliders still double-click to factory
- `MorphTransport` planner: coalesced targets, per-tick byte budget, timestamped MIDI egress
- Opt-in hardware morph probe: `FMLIBPLUG_HW_MORPH_PROBE=1` with `FmLibPlugHwMidiTest`
- Library / morph preset context menu: Open in folder
- Morph parameter lock groups with session defaults in prefs and per morph preset (including lock-reference pad position)
- Morph lock reference pad marker (right-drag); locked groups sample that point
- Morph edge-walk LFO (CW/CCW + rate) paced by the morph transport budget
- Dedicated MIDI controller input; Morph page Note morph: Off / Random / Edges
- Optional forward of controller MIDI to output (default off)
- Library tag filter: collapsible chip panel (Show Tags); Shift+click AND / Ctrl-Cmd+click OR
- Search field filter-token menu; tag chip click toggles `tag:…` AND filter
- Device buffer drag-and-drop slot swap; native folder chooser on Save
- Catch2 coverage for morph transport, lock prefs, tag filter / DNF, and related helpers

### Changed
- Note morph Random/Edges jumps on the first key of a phrase, then delays note-on until SysEx lead-in finishes (audition uses the same lead-in); later chord tones stay polyphonic on that voice
- Morph egress respects **Morph release hold**; note jump and direct pad gestures cancel it
- Minimum editor height raised to 850 px for the Settings checklist
- Morph **Clear** clears corner voices only; **Reset** restores locks and lock reference
- Factory morph locks are **EG + Levels** (`morphLockSchema` 2); Freq lock split into **Coarse** and **Fine**
- Voice morph uses nearest-neighbour for discrete VCED fields plus a silence guard on operator levels
- Full morph voice dumps only when idle (or no baseline); while notes sound, budgeted parameter changes follow the stream mode
- Morph live param-diffs skip voice-name bytes; name is sent via full dump when pad drag ends
- Morph pad requires all four corners before sending; unset corners are dimmed
- Morph drag emit interval defaults to 150 ms (Settings 20–250); click stays a full voice dump
- Morph voice name is one char from each corner plus pad percent (10-char DX7 limit)
- Morph Presets: click/arrow auto-loads; empty preset name field uses the morph name; save selected to Device buffer slot 1–32
- Settings auto-apply on change; MIDI ports still need **Open MIDI ports**
- Favorites is a toolbar toggle; Clear search sits left of Show Tags

### Fixed
- Settings Reset defaults / slider double-click used saved prefs instead of compile-time factories
- Morph SysEx slices no longer overlap on the wire; thru notes count as sounding
- Note morph lead-in stays idle until note-on and covers dumps still queued behind busy wire
- Morph forceCommit clears when idle; bank/voice dumps invalidate the morph baseline without leaving an immortal timer
- Controller-thru note tracking is locked and synced on the message thread
- Queued SysEx and auto-tag callbacks use a shared alive flag so teardown cannot touch freed memory
- Tag OR uses Ctrl or Cmd; Shift+AND after OR expands to DNF (including trailing AND / partial presence)
- Library voice selection no longer sends the SysEx dump twice
- Tag chip clicks use row-relative coordinates; left-click no longer opens the edit dialog
- Tooltips work again (`TooltipWindow`; respects Show tooltips)
- `dupe: OR fav:` (and other flag ORs) use union semantics; search ops are uppercase `AND`/`OR` only

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
