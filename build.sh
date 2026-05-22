#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/env.sh"

cd "$ZEPHYR_WORKSPACE"

west build -p always \
  -b xiao_ble/nrf52840/sense \
  "$APP" \
  --build-dir "$APP/build"