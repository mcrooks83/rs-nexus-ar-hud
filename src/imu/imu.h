#pragma once

#include <stdint.h>

/*
 * IMU sample in packet-ready units.
 *
 *   accel: mg
 *      1000 = 1 g
 *
 *   gyro: mdps
 *      1000 = 1 degree/sec
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