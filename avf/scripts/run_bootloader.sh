#!/usr/bin/env bash
# Push the built bootloader.bin to the Pixel 7 and run it via AVF custom VM config.
# Usage: run_bootloader.sh <path-to-bootloader.bin>
set -euo pipefail

BIN="${1:?usage: run_bootloader.sh <bootloader.bin>}"
REMOTE_DIR="/data/local/tmp/avf-macos"
CFG_LOCAL="$(dirname "$0")/../configs/bootloader_vm.json"
CFG_REMOTE="$REMOTE_DIR/bootloader_vm.json"

export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL="*"

adb shell "mkdir -p $REMOTE_DIR"
adb push "$BIN" "$REMOTE_DIR/bootloader.bin"
adb push "$CFG_LOCAL" "$CFG_REMOTE"

echo "--- launching VM ---"
adb shell "/apex/com.android.virt/bin/vm run $CFG_REMOTE"
