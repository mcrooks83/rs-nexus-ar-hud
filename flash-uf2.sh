#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/env.sh"

UF2="$APP/build/zephyr/zephyr.uf2"
MOUNT="/media/$USER/XIAO-SENSE"

if [ ! -f "$UF2" ]; then
  echo "UF2 not found: $UF2"
  echo "Run ./build.sh first"
  exit 1
fi

if [ ! -d "$MOUNT" ]; then
  echo "XIAO UF2 mount not found: $MOUNT"
  echo "Double-tap reset on the XIAO, then try again."
  exit 1
fi

echo "Copying $UF2 to $MOUNT"
cp "$UF2" "$MOUNT/"
sync

echo "UF2 copied. Board should reboot."