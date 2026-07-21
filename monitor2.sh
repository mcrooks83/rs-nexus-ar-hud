#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/env.sh"

PORT="${ESP_PORT:?ESP_PORT is not set}"
ELF="$APP/build/zephyr/zephyr.elf"

if [[ ! -e "$PORT" ]]; then
    echo "ERROR: serial port not found: $PORT" >&2
    exit 1
fi

if [[ ! -f "$ELF" ]]; then
    echo "ERROR: Zephyr ELF not found: $ELF" >&2
    exit 1
fi

exec python -m esp_idf_monitor \
    --port "$PORT" \
    --baud 115200 \
    --no-reset \
    "$ELF"