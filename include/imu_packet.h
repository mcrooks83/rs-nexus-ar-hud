#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * RS Nexus BLE IMU measurement packet.
 *
 * Transport:
 *   BLE GATT notification from IMU Measurement characteristic.
 *
 * Encoding:
 *   Little-endian raw bytes.
 *
 * Size:
 *   20 bytes exactly.
 *
 * Packet layout:
 *
 *   byte  field
 *   0     version
 *   1     flags
 *   2-3   seq
 *   4-7   timestamp_us
 *   8-9   accel_x
 *   10-11 accel_y
 *   12-13 accel_z
 *   14-15 gyro_x
 *   16-17 gyro_y
 *   18-19 gyro_z
 */

#define IMU_PACKET_VERSION 1
#define IMU_PACKET_SIZE_BYTES 20

#define IMU_PACKET_FLAG_NONE        0x00
#define IMU_PACKET_FLAG_STREAMING   0x01
#define IMU_PACKET_FLAG_ODR_CHANGED 0x02
#define IMU_PACKET_FLAG_IMU_ERROR   0x04

#define IMU_PACKET_VERSION_OFFSET      0
#define IMU_PACKET_FLAGS_OFFSET        1
#define IMU_PACKET_SEQ_OFFSET          2
#define IMU_PACKET_TIMESTAMP_US_OFFSET 4
#define IMU_PACKET_ACCEL_X_OFFSET      8
#define IMU_PACKET_ACCEL_Y_OFFSET      10
#define IMU_PACKET_ACCEL_Z_OFFSET      12
#define IMU_PACKET_GYRO_X_OFFSET       14
#define IMU_PACKET_GYRO_Y_OFFSET       16
#define IMU_PACKET_GYRO_Z_OFFSET       18

static inline void imu_packet_put_u16_le(uint8_t *buf, size_t offset, uint16_t value)
{
    buf[offset] = (uint8_t)(value & 0xff);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xff);
}

static inline void imu_packet_put_i16_le(uint8_t *buf, size_t offset, int16_t value)
{
    imu_packet_put_u16_le(buf, offset, (uint16_t)value);
}

static inline void imu_packet_put_u32_le(uint8_t *buf, size_t offset, uint32_t value)
{
    buf[offset] = (uint8_t)(value & 0xff);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xff);
    buf[offset + 2] = (uint8_t)((value >> 16) & 0xff);
    buf[offset + 3] = (uint8_t)((value >> 24) & 0xff);
}

static inline void imu_packet_build_v1(
    uint8_t *buf,
    uint8_t flags,
    uint16_t seq,
    uint32_t timestamp_us,
    int16_t accel_x,
    int16_t accel_y,
    int16_t accel_z,
    int16_t gyro_x,
    int16_t gyro_y,
    int16_t gyro_z)
{
    buf[IMU_PACKET_VERSION_OFFSET] = IMU_PACKET_VERSION;
    buf[IMU_PACKET_FLAGS_OFFSET] = flags;

    imu_packet_put_u16_le(buf, IMU_PACKET_SEQ_OFFSET, seq);
    imu_packet_put_u32_le(buf, IMU_PACKET_TIMESTAMP_US_OFFSET, timestamp_us);

    imu_packet_put_i16_le(buf, IMU_PACKET_ACCEL_X_OFFSET, accel_x);
    imu_packet_put_i16_le(buf, IMU_PACKET_ACCEL_Y_OFFSET, accel_y);
    imu_packet_put_i16_le(buf, IMU_PACKET_ACCEL_Z_OFFSET, accel_z);

    imu_packet_put_i16_le(buf, IMU_PACKET_GYRO_X_OFFSET, gyro_x);
    imu_packet_put_i16_le(buf, IMU_PACKET_GYRO_Y_OFFSET, gyro_y);
    imu_packet_put_i16_le(buf, IMU_PACKET_GYRO_Z_OFFSET, gyro_z);
}