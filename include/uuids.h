#pragma once

#include <zephyr/bluetooth/uuid.h>

/*
 * RS Nexus BLE IMU custom GATT UUIDs.
 *
 * Base UUID pattern:
 *   8e4xxxxx-f315-4f60-9fb8-838830daea50
 *
 * Services:
 *   8e400001... IMU Measurement Service
 *   8e410001... Device Control Service
 *
 * Characteristics:
 *   8e400002... IMU Measurement notify characteristic
 *   8e410002... Device Control command write characteristic
 *   8e410003... Device Status read characteristic
 */

#define BT_UUID_IMU_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x8e400001, 0xf315, 0x4f60, 0x9fb8, 0x838830daea50)

#define BT_UUID_IMU_MEASUREMENT_VAL \
    BT_UUID_128_ENCODE(0x8e400002, 0xf315, 0x4f60, 0x9fb8, 0x838830daea50)

#define BT_UUID_DEVICE_CONTROL_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x8e410001, 0xf315, 0x4f60, 0x9fb8, 0x838830daea50)

#define BT_UUID_DEVICE_CONTROL_COMMAND_VAL \
    BT_UUID_128_ENCODE(0x8e410002, 0xf315, 0x4f60, 0x9fb8, 0x838830daea50)

#define BT_UUID_DEVICE_STATUS_VAL \
    BT_UUID_128_ENCODE(0x8e410003, 0xf315, 0x4f60, 0x9fb8, 0x838830daea50)

#define BT_UUID_IMU_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_IMU_SERVICE_VAL)

#define BT_UUID_IMU_MEASUREMENT \
    BT_UUID_DECLARE_128(BT_UUID_IMU_MEASUREMENT_VAL)

#define BT_UUID_DEVICE_CONTROL_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_DEVICE_CONTROL_SERVICE_VAL)

#define BT_UUID_DEVICE_CONTROL_COMMAND \
    BT_UUID_DECLARE_128(BT_UUID_DEVICE_CONTROL_COMMAND_VAL)

#define BT_UUID_DEVICE_STATUS \
    BT_UUID_DECLARE_128(BT_UUID_DEVICE_STATUS_VAL)

