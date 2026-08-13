#!/usr/bin/env bash
# Shared helpers for Makefile / install scripts (macOS + Linux; Git Bash on Windows).
# shellcheck disable=SC2034

fmlib_detect_os() {
  case "$(uname -s 2>/dev/null)" in
    Darwin*) echo macos ;;
    Linux*)  echo linux ;;
    MINGW*|MSYS*|CYGWIN*) echo windows ;;
    *) echo unknown ;;
  esac
}

# GNU coreutils `stat -f` is --file-system (not BSD -f format). Detect GNU via --version.
fmlib_stat_is_gnu() {
  stat --version >/dev/null 2>&1
}

# Epoch seconds (portable BSD/GNU).
fmlib_mtime() {
  local f="$1"
  if [ ! -e "$f" ]; then echo 0; return; fi
  if fmlib_stat_is_gnu; then
    stat -c '%Y' "$f"
  else
    stat -f '%m' "$f"
  fi
}

fmlib_mtime_human() {
  local f="$1"
  if [ ! -e "$f" ]; then echo none; return; fi
  if fmlib_stat_is_gnu; then
    # GNU %y is "YYYY-MM-DD HH:MM:SS.ns TZ"; strip fractional seconds / zone.
    stat -c '%y' "$f" | cut -d. -f1
  else
    stat -f '%Sm' -t '%Y-%m-%d %H:%M:%S' "$f"
  fi
}

# Locate SharedCode archive (Unix .a or MSVC .lib).
fmlib_shared_lib() {
  local root="$1"
  for cand in \
    "$root/libFmLibPlug_SharedCode.a" \
    "$root/FmLibPlug_SharedCode.lib" \
    "$root/libFmLibPlug.lib"; do
    if [ -e "$cand" ]; then echo "$cand"; return 0; fi
  done
  return 1
}

# Print first existing non-empty binary under a plugin/standalone product root.
# On Linux/Windows, CLAP is a single ELF/PE file named FmLibPlug.clap (not a bundle).
fmlib_find_binary() {
  local root="$1"
  local cand
  if [ ! -e "$root" ]; then return 1; fi
  if [ -f "$root" ] && [ -s "$root" ]; then
    echo "$root"
    return 0
  fi
  for cand in \
    "$root/Contents/MacOS/FmLibPlug" \
    "$root/Contents/x86_64-linux/FmLibPlug.so" \
    "$root/Contents/aarch64-linux/FmLibPlug.so" \
    "$root/Contents/arm64-linux/FmLibPlug.so" \
    "$root/Contents/x86_64-win/FmLibPlug.vst3" \
    "$root/Contents/arm64-win/FmLibPlug.vst3" \
    "$root/libFmLibPlug.so" \
    "$root/FmLibPlug.so" \
    "$root/FmLibPlug.clap" \
    "$root/FmLibPlug.dll" \
    "$root/FmLibPlug" \
    "$root/FmLibPlug.exe"; do
    if [ -f "$cand" ] && [ -s "$cand" ]; then
      echo "$cand"
      return 0
    fi
  done
  # CLAP / odd layouts: deterministic first match under Contents or root.
  local found
  found="$(find "$root" -type f \( \
      -name 'FmLibPlug' -o -name 'FmLibPlug.exe' -o -name 'FmLibPlug.so' \
      -o -name 'FmLibPlug.dll' -o -name 'libFmLibPlug.so' -o -name 'FmLibPlug.vst3' \
      -o -name 'FmLibPlug.clap' \
    \) 2>/dev/null | LC_ALL=C sort | head -1)"
  if [ -n "$found" ] && [ -s "$found" ]; then
    echo "$found"
    return 0
  fi
  return 1
}

# Default install destinations (override with AU_DIR / VST3_DIR / CLAP_DIR / LV2_DIR).
fmlib_default_dirs() {
  local os
  os="$(fmlib_detect_os)"
  case "$os" in
    macos)
      echo "AU_DIR=${HOME}/Library/Audio/Plug-Ins/Components"
      echo "VST3_DIR=${HOME}/Library/Audio/Plug-Ins/VST3"
      echo "CLAP_DIR=${HOME}/Library/Audio/Plug-Ins/CLAP"
      echo "LV2_DIR=${HOME}/Library/Audio/Plug-Ins/LV2"
      echo "INSTALL_AU=1"
      ;;
    linux)
      echo "AU_DIR="
      echo "VST3_DIR=${HOME}/.vst3"
      echo "CLAP_DIR=${HOME}/.clap"
      echo "LV2_DIR=${HOME}/.lv2"
      echo "INSTALL_AU=0"
      ;;
    windows)
      local pf="${PROGRAMFILES:-/c/Program Files}"
      local common="${COMMONPROGRAMFILES:-$pf/Common Files}"
      echo "AU_DIR="
      echo "VST3_DIR=${common}/VST3"
      echo "CLAP_DIR=${common}/CLAP"
      echo "LV2_DIR=${APPDATA:-$HOME/AppData/Roaming}/LV2"
      echo "INSTALL_AU=0"
      ;;
    *)
      echo "AU_DIR="
      echo "VST3_DIR=${HOME}/.vst3"
      echo "CLAP_DIR=${HOME}/.clap"
      echo "LV2_DIR=${HOME}/.lv2"
      echo "INSTALL_AU=0"
      ;;
  esac
}
