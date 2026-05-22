#pragma once

#include <stdint.h>

/*
 * Device Control Service command protocol.
 *
 * Commands are written as raw little-endian byte payloads.
 *
 * START:
 *   byte 0: opcode = CTRL_START
 *
 * STOP:
 *   byte 0: opcode = CTRL_STOP
 *
 * SET_ODR:
 *   byte 0: opcode = CTRL_SET_ODR
 *   byte 1: odr_hz low byte
 *   byte 2: odr_hz high byte
 *
 * IDENTIFY:
 *   byte 0: opcode = CTRL_IDENTIFY
 *   byte 1: duration_ms low byte
 *   byte 2: duration_ms high byte
 */

enum control_opcode {
    CTRL_START = 0x01,
    CTRL_STOP = 0x02,
    CTRL_SET_ODR = 0x03,
    CTRL_IDENTIFY = 0x04,
    CTRL_GET_STATUS = 0x05,
};

enum imu_odr_hz {
    IMU_ODR_20_HZ = 20,
    IMU_ODR_50_HZ = 50,
    IMU_ODR_100_HZ = 100,
};

#define CONTROL_OPCODE_OFFSET 0

#define CONTROL_START_SIZE_BYTES 1
#define CONTROL_STOP_SIZE_BYTES 1
#define CONTROL_SET_ODR_SIZE_BYTES 3
#define CONTROL_IDENTIFY_SIZE_BYTES 3

#define CONTROL_U16_VALUE_LO_OFFSET 1
#define CONTROL_U16_VALUE_HI_OFFSET 2

#define DEVICE_STATUS_V1_SIZE_BYTES 11
#define DEVICE_STATUS_V2_SIZE_BYTES 19

static inline uint16_t control_get_le16(const uint8_t *buf)
{
    return ((uint16_t)buf[CONTROL_U16_VALUE_LO_OFFSET]) |
           ((uint16_t)buf[CONTROL_U16_VALUE_HI_OFFSET] << 8);
}
