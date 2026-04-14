#!/bin/bash
set -euo pipefail

# Usage: build-pkg.sh <vst3-path> <au-path> <version> [signing-identity]
# Example: build-pkg.sh path/to/MonkSynth.vst3 path/to/MonkSynth.component 0.2.0
#          build-pkg.sh path/to/MonkSynth.vst3 path/to/MonkSynth.component 0.2.0 "Developer ID Installer: ..."

VST3_PATH="${1:?Usage: build-pkg.sh <vst3-path> <au-path> <version> [signing-identity]}"
AU_PATH="${2:?Missing AU path}"
VERSION="${3:?Missing version}"
SIGN_IDENTITY="${4:-}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="$(mktemp -d)"
OUTPUT_DIR="${SCRIPT_DIR}/Output"

cleanup() { rm -rf "$WORK_DIR"; }
trap cleanup EXIT

mkdir -p "$OUTPUT_DIR"

# --- Stage payloads ---
# VST3 goes to /Library/Audio/Plug-Ins/VST3/
VST3_ROOT="$WORK_DIR/vst3-root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$VST3_ROOT"
cp -R "$VST3_PATH" "$VST3_ROOT/"

# AU goes to /Library/Audio/Plug-Ins/Components/
AU_ROOT="$WORK_DIR/au-root/Library/Audio/Plug-Ins/Components"
mkdir -p "$AU_ROOT"
cp -R "$AU_PATH" "$AU_ROOT/"

# Presets go to /Library/Audio/Presets/MonkSynth/MonkSynth/
PRESET_SRC="$SCRIPT_DIR/../../presets"
if [ -d "$PRESET_SRC" ] && ls "$PRESET_SRC"/*.vstpreset >/dev/null 2>&1; then
    PRESET_ROOT="$WORK_DIR/preset-root/Library/Audio/Presets/MonkSynth/MonkSynth"
    mkdir -p "$PRESET_ROOT"
    cp "$PRESET_SRC"/*.vstpreset "$PRESET_ROOT/"
fi

# --- Build component packages ---
pkgbuild \
    --root "$WORK_DIR/vst3-root" \
    --identifier "com.monksynth.vst3" \
    --version "$VERSION" \
    --install-location "/" \
    "$WORK_DIR/MonkSynth-VST3.pkg"

pkgbuild \
    --root "$WORK_DIR/au-root" \
    --identifier "com.monksynth.audiounit" \
    --version "$VERSION" \
    --install-location "/" \
    "$WORK_DIR/MonkSynth-AU.pkg"

if [ -d "$WORK_DIR/preset-root" ]; then
    pkgbuild \
        --root "$WORK_DIR/preset-root" \
        --identifier "com.monksynth.presets" \
        --version "$VERSION" \
        --install-location "/" \
        "$WORK_DIR/MonkSynth-Presets.pkg"
fi

# --- Build product installer ---
if [ -n "$SIGN_IDENTITY" ]; then
    productbuild \
        --distribution "$SCRIPT_DIR/distribution.xml" \
        --package-path "$WORK_DIR" \
        --resources "$SCRIPT_DIR/resources" \
        --version "$VERSION" \
        --sign "$SIGN_IDENTITY" \
        "$OUTPUT_DIR/MonkSynth-${VERSION}-macOS-Setup.pkg"
else
    productbuild \
        --distribution "$SCRIPT_DIR/distribution.xml" \
        --package-path "$WORK_DIR" \
        --resources "$SCRIPT_DIR/resources" \
        --version "$VERSION" \
        "$OUTPUT_DIR/MonkSynth-${VERSION}-macOS-Setup.pkg"
fi

echo "Installer built: $OUTPUT_DIR/MonkSynth-${VERSION}-macOS-Setup.pkg"
