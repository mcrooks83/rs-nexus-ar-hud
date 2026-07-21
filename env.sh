#!/usr/bin/env bash

export APP="$HOME/Desktop/apps/dev/rs-nexus-project/rs-nexus-ar-hud"
export ZEPHYR_WORKSPACE="$HOME/Desktop/zephyr-main"
export ZEPHYR_BASE="$ZEPHYR_WORKSPACE/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-1.0.1"

export ESP_PORT="/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_14:C1:9F:C5:08:3C-if00"

# Needed for nrfutil if installed in ~/.local/bin.
export PATH="$PATH:$HOME/.local/bin"

source "$ZEPHYR_WORKSPACE/.venv/bin/activate"



echo "Using $(python --version)"
echo "python: $(which python)"
echo "west: $(which west)"
echo "nrfutil: $(which nrfutil 2>/dev/null || true)"
echo "ZEPHYR_BASE=$ZEPHYR_BASE"
echo "APP=$APP"
echo "ESP_PORT=$ESP_PORT"