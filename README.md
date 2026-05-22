# RS Nexus BLE IMU

This repository contains the firmware for the `NexusN3Dot` custom IMU sensor.

The device advertises as a BLE peripheral and streams accelerometer and gyroscope data as compact binary notifications. It is intended to be used either directly with a BLE client or through the Nexus BLE gateway tooling.

## Customer Overview

`NexusN3Dot` supports:

- accelerometer and gyroscope streaming
- selectable output data rates of `20 Hz`, `50 Hz`, and `100 Hz`
- custom BLE control commands for `START`, `STOP`, `SET_ODR`, and `IDENTIFY`
- fast BLE connection parameter updates for stable streaming

For customer-facing host usage through the gateway, use the `NexusN3Dot` sample in `rs-nexus-ble-tooling`:

- sample path: `rs-nexus-ble-tooling/NexusN3Dot/stream_client.py`
- recommended default rate: `100 Hz`

Validated gateway flow:

1. connect the sensor
2. subscribe to the IMU measurement characteristic
3. set the output data rate
4. send `START`
5. confirm success by observing incoming stream packets

Important integration note:

- when used through the Nexus BLE gateway, `START` should be treated as a fire-and-observe command
- customers should confirm stream start from incoming notifications and stream statistics rather than depending on a separate start acknowledgement
- this behavior has been validated with the gateway-backed `NexusN3Dot` sample client at `100 Hz`

## Target Hardware

- Seeed Studio XIAO nRF52840 Sense
- Zephyr board target: `xiao_ble`
- IMU: onboard 6-axis accel/gyro
- identify LED: onboard LED, red if available through devicetree

## BLE Features

- custom IMU measurement service
- custom device control service
- binary accel/gyro packets
- output data rates:
  - `20 Hz`
  - `50 Hz`
  - `100 Hz`
- `START` command
- `STOP` command
- `SET_ODR` command
- `IDENTIFY` command

## Build

```bash
./build.sh
```

## Flash

```bash
./flash.sh
```

## Repository Layout

`include/`
Shared protocol headers.

`src/ble/`
BLE advertising, connection management, GATT services, and notifications.

`src/streaming/`
Streaming state machine and sample timer.

`src/imu/`
IMU initialization and accel/gyro reads.

`src/led/`
Identify LED behavior.

`tools/`
Host-side test utilities, including the direct Bleak validation client.

`docs/`
Protocol and bring-up notes.
