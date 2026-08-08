#!/usr/bin/env bash
# Install host plugin formats from the artefact root into OS-default folders.
# Usage: scripts/install-plugins.sh <ARTEFACT_ROOT>
# Env overrides: AU_DIR VST3_DIR CLAP_DIR LV2_DIR INSTALL_AU=0|1
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=fmlib-paths.sh
source "$ROOT/scripts/fmlib-paths.sh"

ARTEFACT_ROOT="${1:-}"
if [ -z "$ARTEFACT_ROOT" ] || [ ! -d "$ARTEFACT_ROOT" ]; then
  echo "Usage: $0 <ARTEFACT_ROOT>" >&2
  exit 1
fi

# Apply OS defaults only when the caller did not set the variable.
while IFS='=' read -r key val; do
  case "$key" in
    AU_DIR) : "${AU_DIR:=$val}" ;;
    VST3_DIR) : "${VST3_DIR:=$val}" ;;
    CLAP_DIR) : "${CLAP_DIR:=$val}" ;;
    LV2_DIR) : "${LV2_DIR:=$val}" ;;
    INSTALL_AU) : "${INSTALL_AU:=$val}" ;;
  esac
done < <(fmlib_default_dirs)

INSTALL_AU="${INSTALL_AU:-0}"

OS="$(fmlib_detect_os)"
echo ""
echo "==== INSTALL ($OS) ===="
echo "Source:  $ARTEFACT_ROOT/"
echo "Destinations:"
if [ "$INSTALL_AU" = "1" ]; then
  echo "  AU    -> ${AU_DIR}/FmLibPlug.component"
fi
echo "  VST3  -> ${VST3_DIR}/FmLibPlug.vst3"
echo "  CLAP  -> ${CLAP_DIR}/FmLibPlug.clap"
echo "  LV2   -> ${LV2_DIR}/FmLibPlug.lv2"
echo "==============="

copy_tree() {
  local src="$1" dst="$2"
  if [ ! -e "$src" ]; then
    echo "SKIP (missing artefact): $src"
    return 1
  fi
  mkdir -p "$(dirname "$dst")"
  rm -rf "$dst"
  if command -v ditto >/dev/null 2>&1; then
    ditto "$src" "$dst"
  else
    cp -a "$src" "$dst"
  fi
  return 0
}

fail=0
if [ "$INSTALL_AU" = "1" ]; then
  if ! copy_tree "$ARTEFACT_ROOT/AU/FmLibPlug.component" "$AU_DIR/FmLibPlug.component"; then
    echo "ERROR: AU artefact missing but INSTALL_AU=1" >&2
    exit 1
  fi
fi
copy_tree "$ARTEFACT_ROOT/VST3/FmLibPlug.vst3" "$VST3_DIR/FmLibPlug.vst3" || fail=1
copy_tree "$ARTEFACT_ROOT/CLAP/FmLibPlug.clap" "$CLAP_DIR/FmLibPlug.clap" || fail=1
copy_tree "$ARTEFACT_ROOT/LV2/FmLibPlug.lv2" "$LV2_DIR/FmLibPlug.lv2" || fail=1

if [ -f "$ARTEFACT_ROOT/BUILD_STAMP.txt" ]; then
  cp "$ARTEFACT_ROOT/BUILD_STAMP.txt" "$VST3_DIR/FmLibPlug.vst3/BUILD_STAMP.txt" 2>/dev/null || true
  if [ "$INSTALL_AU" = "1" ]; then
    cp "$ARTEFACT_ROOT/BUILD_STAMP.txt" "$AU_DIR/FmLibPlug.component/BUILD_STAMP.txt" 2>/dev/null || true
  fi
fi

echo "---- checksum verification ----"
verify_copy() {
  local src_root="$1" dst_root="$2" label="$3"
  local src dst
  src="$(fmlib_find_binary "$src_root" || true)"
  dst="$(fmlib_find_binary "$dst_root" || true)"
  if [ -z "$src" ] || [ -z "$dst" ]; then
    echo "MISSING $label"
    echo "  src=$src_root"
    echo "  dst=$dst_root"
    fail=1
    return
  fi
  local ss ds
  if command -v shasum >/dev/null 2>&1; then
    ss="$(shasum -a 256 "$src" | awk '{print $1}')"
    ds="$(shasum -a 256 "$dst" | awk '{print $1}')"
  else
    ss="$(sha256sum "$src" | awk '{print $1}')"
    ds="$(sha256sum "$dst" | awk '{print $1}')"
  fi
  local dm
  dm="$(fmlib_mtime_human "$dst")"
  if [ "$ss" = "$ds" ]; then
    echo "OK  $label  $dm"
    echo "    $dst"
  else
    echo "MISMATCH $label"
    fail=1
  fi
}

if [ "$INSTALL_AU" = "1" ]; then
  verify_copy "$ARTEFACT_ROOT/AU/FmLibPlug.component" "$AU_DIR/FmLibPlug.component" "AU"
fi
verify_copy "$ARTEFACT_ROOT/VST3/FmLibPlug.vst3" "$VST3_DIR/FmLibPlug.vst3" "VST3"
verify_copy "$ARTEFACT_ROOT/CLAP/FmLibPlug.clap" "$CLAP_DIR/FmLibPlug.clap" "CLAP"
verify_copy "$ARTEFACT_ROOT/LV2/FmLibPlug.lv2" "$LV2_DIR/FmLibPlug.lv2" "LV2"

if [ "$fail" != "0" ]; then
  echo "Install verification failed." >&2
  exit 1
fi

echo "Install complete. Quit the DAW fully (not just rescan) — hosts keep loaded plugins in memory."
echo "Confirm the UI version label matches the Build id printed by make build."

if [ "$OS" = "macos" ]; then
  rm -f "$HOME/Library/Caches/com.apple.audiounits.cache" 2>/dev/null || true
  rm -rf "$HOME/Library/Caches/AudioUnitCache" 2>/dev/null || true
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  codesign --force --deep -s - "$AU_DIR/FmLibPlug.component" 2>/dev/null || true
  codesign --force --deep -s - "$VST3_DIR/FmLibPlug.vst3" 2>/dev/null || true
  codesign --force --deep -s - "$CLAP_DIR/FmLibPlug.clap" 2>/dev/null || true
  buildver="$(date -u +%Y%m%d%H%M%S)"
  for plist in \
    "$AU_DIR/FmLibPlug.component/Contents/Info.plist" \
    "$VST3_DIR/FmLibPlug.vst3/Contents/Info.plist" \
    "$CLAP_DIR/FmLibPlug.clap/Contents/Info.plist"; do
    if [ -f "$plist" ]; then
      /usr/libexec/PlistBuddy -c "Set :CFBundleVersion $buildver" "$plist" 2>/dev/null \
        || /usr/libexec/PlistBuddy -c "Add :CFBundleVersion string $buildver" "$plist" 2>/dev/null \
        || true
    fi
  done
fi

standalone="$(fmlib_find_binary "$ARTEFACT_ROOT/Standalone/FmLibPlug.app" 2>/dev/null || true)"
if [ -z "$standalone" ]; then
  standalone="$(fmlib_find_binary "$ARTEFACT_ROOT/Standalone" 2>/dev/null || true)"
fi
if [ -n "$standalone" ]; then
  echo "Standalone (not installed): $standalone"
else
  echo "Standalone (not installed): under $ARTEFACT_ROOT/Standalone/"
fi
