#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p \
  "$ROOT/boards" \
  "$ROOT/include" \
  "$ROOT/src/ble" \
  "$ROOT/src/imu" \
  "$ROOT/src/streaming" \
  "$ROOT/src/led" \
  "$ROOT/src/util" \
  "$ROOT/tools" \
  "$ROOT/docs"

touch \
  "$ROOT/README.md" \
  "$ROOT/CMakeLists.txt" \
  "$ROOT/prj.conf" \
  "$ROOT/VERSION" \
  "$ROOT/env.sh" \
  "$ROOT/build.sh" \
  "$ROOT/flash.sh" \
  "$ROOT/boards/xiao_ble.overlay" \
  "$ROOT/include/app_version.h" \
  "$ROOT/include/imu_packet.h" \
  "$ROOT/include/control_protocol.h" \
  "$ROOT/include/uuids.h" \
  "$ROOT/src/main.c" \
  "$ROOT/src/ble/ble_services.c" \
  "$ROOT/src/ble/ble_services.h" \
  "$ROOT/src/ble/imu_measurement_service.c" \
  "$ROOT/src/ble/imu_measurement_service.h" \
  "$ROOT/src/ble/device_control_service.c" \
  "$ROOT/src/ble/device_control_service.h" \
  "$ROOT/src/imu/imu.c" \
  "$ROOT/src/imu/imu.h" \
  "$ROOT/src/streaming/streamer.c" \
  "$ROOT/src/streaming/streamer.h" \
  "$ROOT/src/led/identify_led.c" \
  "$ROOT/src/led/identify_led.h" \
  "$ROOT/src/util/timebase.c" \
  "$ROOT/src/util/timebase.h" \
  "$ROOT/tools/ble_client_test.py" \
  "$ROOT/tools/decode_packet.py" \
  "$ROOT/docs/protocol.md" \
  "$ROOT/docs/gatt.md" \
  "$ROOT/docs/bringup.md"

chmod +x \
  "$ROOT/bootstrap.sh" \
  "$ROOT/env.sh" \
  "$ROOT/build.sh" \
  "$ROOT/flash.sh" \
  "$ROOT/tools/ble_client_test.py" \
  "$ROOT/tools/decode_packet.py"

echo "Scaffolded repo at: $ROOT"
echo
find "$ROOT" -maxdepth 3 -type f | sort