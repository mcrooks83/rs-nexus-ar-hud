#include "imu.h"

#include <errno.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

static uint16_t current_odr_hz = 100;
static int16_t fake_counter;

int imu_init(void)
{
    /*
     * Placeholder backend.
     *
     * Later this will:
     *   - bind to the LSM6DS3TR-C sensor device
     *   - verify device_is_ready()
     *   - configure accel/gyro ranges
     *   - configure output data rate
     */
    LOG_INF("IMU placeholder initialized");
    return 0;
}

int imu_set_odr_hz(uint16_t odr_hz)
{
    switch (odr_hz) {
    case 20:
    case 50:
    case 100:
        current_odr_hz = odr_hz;
        LOG_INF("IMU ODR set to %u Hz", current_odr_hz);
        return 0;

    default:
        LOG_WRN("Unsupported IMU ODR: %u", odr_hz);
        return -EINVAL;
    }
}

int imu_read(struct imu_sample *sample)
{
    if (!sample) {
        return -EINVAL;
    }

    fake_counter++;

    sample->accel_x = fake_counter;
    sample->accel_y = fake_counter + 1;
    sample->accel_z = fake_counter + 2;

    sample->gyro_x = fake_counter + 3;
    sample->gyro_y = fake_counter + 4;
    sample->gyro_z = fake_counter + 5;

    return 0;
}