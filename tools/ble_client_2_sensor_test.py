#!/usr/bin/env python3

import asyncio
import struct
import time
from dataclasses import dataclass, field

from bleak import BleakClient, BleakScanner

DEVICE_NAME = "Nexus N3 Dot"

IMU_MEASUREMENT_UUID = "8e400002-f315-4f60-9fb8-838830daea50"
CONTROL_COMMAND_UUID = "8e410002-f315-4f60-9fb8-838830daea50"

CTRL_START = 0x01
CTRL_STOP = 0x02
CTRL_SET_ODR = 0x03
CTRL_IDENTIFY = 0x04

PACKET = struct.Struct("<BBHIhhhhhh")

SENSOR_COUNT = 2
SCAN_TIMEOUT_S = 10.0
CONNECT_SETTLE_S = 2.0
PRE_START_S = 2.0
STREAM_S = 10.0
POST_STOP_S = 2.0


@dataclass
class SensorState:
    address: str
    name: str
    packet_count: int = 0
    start_time: float | None = None
    last_seq: int | None = None
    gap_events: int = 0
    estimated_drops: int = 0
    first_packet_time: float | None = None
    latest_line_time: float = field(default_factory=time.monotonic)


def make_packet_handler(state: SensorState):
    def handle_packet(sender, data):
        now = time.monotonic()

        if state.start_time is None:
            state.start_time = now

        if state.first_packet_time is None:
            state.first_packet_time = now

        state.packet_count += 1

        if len(data) != PACKET.size:
            print(f"{state.address} bad packet size: {len(data)}")
            return

        version, flags, seq, timestamp_us, ax, ay, az, gx, gy, gz = PACKET.unpack(data)

        if state.last_seq is not None:
            expected = (state.last_seq + 1) & 0xFFFF
            if seq != expected:
                diff = (seq - expected) & 0xFFFF
                state.gap_events += 1
                state.estimated_drops += diff
                print(
                    f"{state.address} GAP last={state.last_seq} "
                    f"expected={expected} got={seq} drops~={diff}"
                )

        state.last_seq = seq

        # Print about 5 Hz per sensor to avoid flooding the console.
        if now - state.latest_line_time >= 0.2:
            elapsed = now - state.start_time
            rate = state.packet_count / elapsed if elapsed > 0 else 0.0
            state.latest_line_time = now

            print(
                f"{state.address} "
                f"seq={seq:5d} "
                f"rate={rate:6.1f} Hz "
                f"t={timestamp_us} "
                f"accel=({ax},{ay},{az}) "
                f"gyro=({gx},{gy},{gz})"
            )

    return handle_packet


async def discover_sensors() -> list:
    print(f"Scanning for {DEVICE_NAME!r}...")

    devices = await BleakScanner.discover(timeout=SCAN_TIMEOUT_S)

    matches = []
    for device in devices:
        if device.name == DEVICE_NAME:
            print(f"SCAN MATCH: {device.address} name={device.name} rssi={getattr(device, 'rssi', None)}")
            matches.append(device)

    if len(matches) < SENSOR_COUNT:
        raise RuntimeError(
            f"Found {len(matches)} {DEVICE_NAME!r} device(s), need {SENSOR_COUNT}"
        )

    selected = matches[:SENSOR_COUNT]
    print("Selected:", [d.address for d in selected])
    return selected


async def configure_sensor(client: BleakClient, state: SensorState):
    print(f"{state.address} pre-stop")
    await client.write_gatt_char(
        CONTROL_COMMAND_UUID,
        bytes([CTRL_STOP]),
        response=True,
    )

    print(f"{state.address} subscribe")
    await client.start_notify(
        IMU_MEASUREMENT_UUID,
        make_packet_handler(state),
    )

    print(f"{state.address} set ODR 100 Hz")
    await client.write_gatt_char(
        CONTROL_COMMAND_UUID,
        struct.pack("<BH", CTRL_SET_ODR, 100),
        response=True,
    )


async def run_sensor(device, state: SensorState):
    print(f"{state.address} connecting...")

    async with BleakClient(device) as client:
        print(f"{state.address} connected")

        await asyncio.sleep(CONNECT_SETTLE_S)

        await configure_sensor(client, state)

        print(f"{state.address} configured")

        # Keep connection open until main controls start/stop.
        while True:
            await asyncio.sleep(1.0)


async def main():
    devices = await discover_sensors()

    states = [
        SensorState(address=device.address, name=device.name or DEVICE_NAME)
        for device in devices
    ]

    clients: list[BleakClient] = []

    try:
        print("Connecting both sensors...")

        for device, state in zip(devices, states):
            client = BleakClient(device)
            await client.connect()
            print(f"{state.address} connected")
            clients.append(client)

        print(f"Sleeping {CONNECT_SETTLE_S:.1f}s after connect")
        await asyncio.sleep(CONNECT_SETTLE_S)

        print("Configuring both sensors: pre-stop, subscribe, set-rate")

        for client, state in zip(clients, states):
            await configure_sensor(client, state)
            await asyncio.sleep(0.75)

        print(f"Sleeping {PRE_START_S:.1f}s before START")
        await asyncio.sleep(PRE_START_S)

        print("Optional identify on both sensors")
        for client, state in zip(clients, states):
            print(f"{state.address} identify")
            await client.write_gatt_char(
                CONTROL_COMMAND_UUID,
                bytes([CTRL_IDENTIFY]),
                response=True,
            )

        await asyncio.sleep(6.0)

        print("Starting both streams")
        for client, state in zip(clients, states):
            print(f"{state.address} start")
            await client.write_gatt_char(
                CONTROL_COMMAND_UUID,
                bytes([CTRL_START]),
                response=True,
            )

        print(f"Streaming for {STREAM_S:.1f}s")
        await asyncio.sleep(STREAM_S)

        print("Stopping both streams")
        for client, state in zip(clients, states):
            print(f"{state.address} stop")
            await client.write_gatt_char(
                CONTROL_COMMAND_UUID,
                bytes([CTRL_STOP]),
                response=True,
            )

        print(f"Sleeping {POST_STOP_S:.1f}s after STOP")
        await asyncio.sleep(POST_STOP_S)

        print("Unsubscribing")
        for client, state in zip(clients, states):
            try:
                await client.stop_notify(IMU_MEASUREMENT_UUID)
            except Exception as exc:
                print(f"{state.address} stop_notify warning: {exc}")

    finally:
        print("Disconnecting")
        for client, state in zip(clients, states):
            if client.is_connected:
                await client.disconnect()
                print(f"{state.address} disconnected")

    print()
    print("Summary")
    for state in states:
        elapsed = 0.0
        if state.start_time is not None:
            elapsed = time.monotonic() - state.start_time

        rate = state.packet_count / elapsed if elapsed > 0 else 0.0

        print(
            f"{state.address} "
            f"packets={state.packet_count} "
            f"rate={rate:.2f}Hz "
            f"gap_events={state.gap_events} "
            f"estimated_drops={state.estimated_drops}"
        )


if __name__ == "__main__":
    asyncio.run(main())