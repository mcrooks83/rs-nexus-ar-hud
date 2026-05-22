#include "imu.h"

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

#define IMU_NODE DT_NODELABEL(lsm6ds3tr_c)

#define GYRO_CALIBRATION_DISCARD_SAMPLES 64
#define GYRO_CALIBRATION_SAMPLES 256
#define GYRO_CALIBRATION_DELAY_MS 5

static int32_t gyro_bias_mdps_x;
static int32_t gyro_bias_mdps_y;
static int32_t gyro_bias_mdps_z;
static bool gyro_calibrated;

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

static int gyro_values_to_mdps(
    const struct sensor_value gyro[3],
    int32_t *x_mdps,
    int32_t *y_mdps,
    int32_t *z_mdps)
{
    int32_t gx_mrad_s;
    int32_t gy_mrad_s;
    int32_t gz_mrad_s;

    if (!gyro || !x_mdps || !y_mdps || !z_mdps) {
        return -EINVAL;
    }

    /*
     * Zephyr gyro values are rad/s.
     * rs_sensor_value_to_milli() gives milli-rad/s.
     * mdps = milli-rad/s * 57296 / 1000.
     */
    gx_mrad_s = rs_sensor_value_to_milli(&gyro[0]);
    gy_mrad_s = rs_sensor_value_to_milli(&gyro[1]);
    gz_mrad_s = rs_sensor_value_to_milli(&gyro[2]);

    *x_mdps = gx_mrad_s * 57296 / 1000;
    *y_mdps = gy_mrad_s * 57296 / 1000;
    *z_mdps = gz_mrad_s * 57296 / 1000;

    return 0;
}

static int calibrate_gyro_bias(void)
{
    struct sensor_value gyro[3];
    int64_t sum_x = 0;
    int64_t sum_y = 0;
    int64_t sum_z = 0;
    int valid_samples = 0;
    int err;

    LOG_INF("Starting gyro calibration; keep device still");

    gyro_bias_mdps_x = 0;
    gyro_bias_mdps_y = 0;
    gyro_bias_mdps_z = 0;
    gyro_calibrated = false;

    for (int i = 0; i < GYRO_CALIBRATION_DISCARD_SAMPLES; i++) {
        sensor_sample_fetch(imu_dev);
        k_sleep(K_MSEC(GYRO_CALIBRATION_DELAY_MS));
    }

    for (int i = 0; i < GYRO_CALIBRATION_SAMPLES; i++) {
        int32_t gx_mdps;
        int32_t gy_mdps;
        int32_t gz_mdps;

        err = sensor_sample_fetch(imu_dev);
        if (err) {
            LOG_WRN("Calibration sample fetch failed: %d", err);
            k_sleep(K_MSEC(GYRO_CALIBRATION_DELAY_MS));
            continue;
        }

        err = sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
        if (err) {
            LOG_WRN("Calibration gyro read failed: %d", err);
            k_sleep(K_MSEC(GYRO_CALIBRATION_DELAY_MS));
            continue;
        }

        err = gyro_values_to_mdps(gyro, &gx_mdps, &gy_mdps, &gz_mdps);
        if (err) {
            return err;
        }

        sum_x += gx_mdps;
        sum_y += gy_mdps;
        sum_z += gz_mdps;
        valid_samples++;

        k_sleep(K_MSEC(GYRO_CALIBRATION_DELAY_MS));
    }

    if (valid_samples < GYRO_CALIBRATION_SAMPLES / 2) {
        LOG_WRN("Gyro calibration failed: only %d valid samples", valid_samples);
        return -EIO;
    }

    gyro_bias_mdps_x = (int32_t)(sum_x / valid_samples);
    gyro_bias_mdps_y = (int32_t)(sum_y / valid_samples);
    gyro_bias_mdps_z = (int32_t)(sum_z / valid_samples);
    gyro_calibrated = true;

    LOG_INF(
        "Gyro bias mdps: x=%d y=%d z=%d samples=%d",
        gyro_bias_mdps_x,
        gyro_bias_mdps_y,
        gyro_bias_mdps_z,
        valid_samples
    );

    return 0;
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

    LOG_INF("Waiting for IMU to settle before gyro calibration");
    k_sleep(K_MSEC(500));

    err = calibrate_gyro_bias();
    if (err) {
        LOG_WRN("Gyro calibration failed, continuing without bias correction: %d", err);
    }

    LOG_INF("IMU initialized: %s", imu_dev->name);

    return 0;
}

int imu_set_odr_hz(uint16_t odr_hz)
{
    int err;
    bool odr_changed;

    switch (odr_hz) {
    case 20:
    case 50:
    case 100:
        break;

    default:
        LOG_WRN("Unsupported IMU ODR: %u", odr_hz);
        return -EINVAL;
    }

    odr_changed = (odr_hz != current_odr_hz);

    err = configure_odr(odr_hz);
    if (err) {
        return err;
    }

    current_odr_hz = odr_hz;

    if (odr_changed) {
        gyro_calibrated = false;
        gyro_bias_mdps_x = 0;
        gyro_bias_mdps_y = 0;
        gyro_bias_mdps_z = 0;

        LOG_INF("IMU ODR request set to %u Hz; gyro calibration cleared", current_odr_hz);
    } else {
        LOG_INF("IMU ODR request set to %u Hz", current_odr_hz);
    }

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
    int32_t gx_mdps_32;
    int32_t gy_mdps_32;
    int32_t gz_mdps_32;

    err = gyro_values_to_mdps(gyro, &gx_mdps_32, &gy_mdps_32, &gz_mdps_32);
    if (err) {
        return err;
    }

    if (gyro_calibrated) {
        gx_mdps_32 -= gyro_bias_mdps_x;
        gy_mdps_32 -= gyro_bias_mdps_y;
        gz_mdps_32 -= gyro_bias_mdps_z;
    }

    gx_mdps = gx_mdps_32;
    gy_mdps = gy_mdps_32;
    gz_mdps = gz_mdps_32;

    sample->accel_x = (int16_t)clamp_i16(ax_mg);
    sample->accel_y = (int16_t)clamp_i16(ay_mg);
    sample->accel_z = (int16_t)clamp_i16(az_mg);

    sample->gyro_x = (int16_t)clamp_i16(gx_mdps);
    sample->gyro_y = (int16_t)clamp_i16(gy_mdps);
    sample->gyro_z = (int16_t)clamp_i16(gz_mdps);

    return 0;
}