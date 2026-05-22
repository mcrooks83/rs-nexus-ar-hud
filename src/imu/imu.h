#pragma once

#include <stdint.h>

/*
 * IMU sample in firmware-internal units.
 *
 * For initial bring-up this may be fake data or raw sensor counts.
 * Before app/client integration, we should lock the units to:
 *
 *   accel: mg
 *   gyro:  mdps
 */
struct imu_sample {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

int imu_init(void);
int imu_set_odr_hz(uint16_t odr_hz);
int imu_read(struct imu_sample *sample);