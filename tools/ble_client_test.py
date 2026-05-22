#!/usr/bin/env python3

import asyncio
import struct
import time

from bleak import BleakClient, BleakScanner

CONNECT_SETTLE_S = 2.0
PRE_START_S = 2.0
STREAM_S = 10.0
POST_STOP_S = 2.0

DEVICE_NAME = "Nexus N3 Dot"

IMU_MEASUREMENT_UUID = "8e400002-f315-4f60-9fb8-838830daea50"
CONTROL_COMMAND_UUID = "8e410002-f315-4f60-9fb8-838830daea50"

CTRL_START = 0x01
CTRL_STOP = 0x02
CTRL_SET_ODR = 0x03

PACKET = struct.Struct("<BBHIhhhhhh")

packet_count = 0
start_time = None


def handle_packet(sender, data):
    global packet_count, start_time

    if start_time is None:
        start_time = time.monotonic()

    packet_count += 1

    if len(data) != PACKET.size:
        print(f"bad packet size: {len(data)}")
        return

    version, flags, seq, timestamp_us, ax, ay, az, gx, gy, gz = PACKET.unpack(data)

    if packet_count % 25 == 0:
        elapsed = time.monotonic() - start_time
        rate = packet_count / elapsed if elapsed > 0 else 0.0

        print(
            f"seq={seq:5d} "
            f"rate={rate:6.1f} Hz "
            f"t={timestamp_us} "
            f"accel=({ax},{ay},{az}) "
            f"gyro=({gx},{gy},{gz})"
        )


async def main():
    print(f"Scanning for {DEVICE_NAME!r}...")

    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: d.name == DEVICE_NAME,
        timeout=10.0,
    )

    if device is None:
        raise RuntimeError(f"Could not find BLE device named {DEVICE_NAME!r}")

    print(f"Found {device.address}")
    print("Connecting...")

    async with BleakClient(device) as client:
        print("Connected")
        print(f"Sleeping {CONNECT_SETTLE_S:.1f}s after connect")
        await asyncio.sleep(CONNECT_SETTLE_S)

        print("Subscribing to IMU notifications")
        await client.start_notify(IMU_MEASUREMENT_UUID, handle_packet)

        print(f"Sleeping {PRE_START_S:.1f}s before START")
        await asyncio.sleep(PRE_START_S)

        print("Setting ODR to 100 Hz")
        await client.write_gatt_char(
            CONTROL_COMMAND_UUID,
            struct.pack("<BH", CTRL_SET_ODR, 100),
            response=True,
        )

        print("Starting stream")
        await client.write_gatt_char(
            CONTROL_COMMAND_UUID,
            bytes([CTRL_START]),
            response=True,
        )

        print(f"Streaming for {STREAM_S:.1f}s")
        await asyncio.sleep(STREAM_S)

        print("Stopping stream")
        await client.write_gatt_char(
            CONTROL_COMMAND_UUID,
            bytes([CTRL_STOP]),
            response=True,
        )

        print(f"Sleeping {POST_STOP_S:.1f}s after STOP")
        await asyncio.sleep(POST_STOP_S)

        print("Unsubscribing from IMU notifications")
        await client.stop_notify(IMU_MEASUREMENT_UUID)

        print("Disconnecting...")

    print("Disconnected")

if __name__ == "__main__":
    asyncio.run(main())