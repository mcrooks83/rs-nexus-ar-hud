# RS Nexus BLE IMU

Zephyr firmware for the Seeed Studio XIAO nRF52840 Sense.

The device advertises as a BLE peripheral and streams accelerometer + gyroscope data as compact binary notifications.

## Target hardware

- Seeed Studio XIAO nRF52840 Sense
- Zephyr board target: `xiao_ble`
- IMU: onboard 6-axis accel/gyro
- Identify LED: onboard LED, red if available through devicetree

## Features

- BLE peripheral
- Custom IMU measurement service
- Custom device control service
- Binary accel/gyro packets
- Output data rates:
  - 20 Hz
  - 50 Hz
  - 100 Hz
- START command
- STOP command
- SET_ODR command
- IDENTIFY command
- Fast BLE connection parameter request
- Max TX power configuration where supported

## Build

```bash
./build.sh
```

## Flash
```bash
./flash.sh
```
## Repo Layout
include/
  Shared protocol headers.

src/ble/
  BLE advertising, connection management, GATT services, notifications.

src/streaming/
  Streaming state machine and sample timer.

src/imu/
  IMU initialization and accel/gyro reads.

src/led/
  Identify LED behavior.

tools/
  Host-side test utilities.

docs/
  Protocol and bring-up notes.

