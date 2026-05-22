#include "imu.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

#define IMU_NODE DT_NODELABEL(lsm6ds3tr_c)

#if !DT_NODE_HAS_STATUS(IMU_NODE, okay)
#error "LSM6DS3TR-C IMU node is not okay in devicetree"
#endif

static const struct device *imu_dev = DEVICE_DT_GET(IMU_NODE);

static uint16_t current_odr_hz = 100;

static int rs_sensor_value_to_milli(const struct sensor_value *val)
{
    return (int)((int64_t)val->val1 * 1000 + val->val2 / 1000);
}

static int clamp_i16(int value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }

    if (value < INT16_MIN) {
        return INT16_MIN;
    }

    return value;
}

static int configure_odr(uint16_t requested_odr_hz)
{
    struct sensor_value odr;
    int err;

    switch (requested_odr_hz) {
    case 20:
        odr.val1 = 26;
        odr.val2 = 0;
        break;

    case 50:
        odr.val1 = 52;
        odr.val2 = 0;
        break;

    case 100:
        odr.val1 = 104;
        odr.val2 = 0;
        break;

    default:
        return -EINVAL;
    }

    err = sensor_attr_set(
        imu_dev,
        SENSOR_CHAN_ACCEL_XYZ,
        SENSOR_ATTR_SAMPLING_FREQUENCY,
        &odr
    );
    if (err) {
        LOG_WRN(
            "Failed to set accel ODR request %u Hz -> %d.%06d Hz: %d",
            requested_odr_hz,
            odr.val1,
            odr.val2,
            err
        );
        return err;
    }

    err = sensor_attr_set(
        imu_dev,
        SENSOR_CHAN_GYRO_XYZ,
        SENSOR_ATTR_SAMPLING_FREQUENCY,
        &odr
    );
    if (err) {
        LOG_WRN(
            "Failed to set gyro ODR request %u Hz -> %d.%06d Hz: %d",
            requested_odr_hz,
            odr.val1,
            odr.val2,
            err
        );
        return err;
    }

    LOG_INF(
        "Configured IMU ODR request %u Hz -> hardware %d.%06d Hz",
        requested_odr_hz,
        odr.val1,
        odr.val2
    );

    return 0;
}

int imu_init(void)
{
    int err;

    if (!device_is_ready(imu_dev)) {
        LOG_ERR("IMU device is not ready");
        return -ENODEV;
    }

    LOG_INF("IMU device ready: %s", imu_dev->name);

    err = configure_odr(current_odr_hz);
    if (err) {
        LOG_ERR("ODR config failed: %d", err);
        return err;
    }

    LOG_INF("IMU initialized: %s", imu_dev->name);

    return 0;
}

int imu_set_odr_hz(uint16_t odr_hz)
{
    int err;

    switch (odr_hz) {
    case 20:
    case 50:
    case 100:
        break;

    default:
        LOG_WRN("Unsupported IMU ODR: %u", odr_hz);
        return -EINVAL;
    }

    err = configure_odr(odr_hz);
    if (err) {
        return err;
    }

    current_odr_hz = odr_hz;

    LOG_INF("IMU ODR request set to %u Hz", current_odr_hz);

    return 0;
}
int imu_read(struct imu_sample *sample)
{
    struct sensor_value accel[3];
    struct sensor_value gyro[3];
    int ax_mg;
    int ay_mg;
    int az_mg;
    int gx_mdps;
    int gy_mdps;
    int gz_mdps;
    int err;

    if (!sample) {
        return -EINVAL;
    }

    err = sensor_sample_fetch(imu_dev);
    if (err) {
        LOG_WRN("sensor_sample_fetch failed: %d", err);
        return err;
    }

    err = sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (err) {
        LOG_WRN("sensor_channel_get accel failed: %d", err);
        return err;
    }

    err = sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
    if (err) {
        LOG_WRN("sensor_channel_get gyro failed: %d", err);
        return err;
    }

    static uint32_t log_count;

    /*
     * Zephyr accel values are in m/s^2.
     * Convert to approximate mg:
     *
     *   1 g = 9.80665 m/s^2
     *   mg = m/s^2 * 1000 / 9.80665
     *
     * sensor_value_to_milli() gives milli m/s^2.
     */
    ax_mg = rs_sensor_value_to_milli(&accel[0]) * 1000 / 9807;
    ay_mg = rs_sensor_value_to_milli(&accel[1]) * 1000 / 9807;
    az_mg = rs_sensor_value_to_milli(&accel[2]) * 1000 / 9807;

    /*
     * Zephyr gyro values are in rad/s.
     * Convert to approximate mdps:
     *
     *   deg/s = rad/s * 57.2957795
     *   mdps = rad/s * 57295.7795
     *
     * rs_sensor_value_to_milli() gives milli rad/s.
     * mdps ~= milli_rad_s * 57296 / 1000
     */
    gx_mdps = rs_sensor_value_to_milli(&gyro[0]) * 57296 / 1000;
    gy_mdps = rs_sensor_value_to_milli(&gyro[1]) * 57296 / 1000;
    gz_mdps = rs_sensor_value_to_milli(&gyro[2]) * 57296 / 1000;

    sample->accel_x = (int16_t)clamp_i16(ax_mg);
    sample->accel_y = (int16_t)clamp_i16(ay_mg);
    sample->accel_z = (int16_t)clamp_i16(az_mg);

    sample->gyro_x = (int16_t)clamp_i16(gx_mdps);
    sample->gyro_y = (int16_t)clamp_i16(gy_mdps);
    sample->gyro_z = (int16_t)clamp_i16(gz_mdps);

    return 0;
}