#include "streamer.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "imu.h"
#include "imu_packet.h"
#include "imu_measurement_service.h"
#include "status_led.h"
#include "timebase.h"

LOG_MODULE_REGISTER(streamer, LOG_LEVEL_INF);

static struct k_timer stream_timer;
static struct k_work stream_work;
static struct k_work odr_work;

static bool running;
static uint16_t odr_hz = 100;
static uint16_t pending_imu_odr_hz = 100;
static uint16_t seq;

static uint32_t packets_sent;
static uint32_t packets_dropped;
static uint32_t imu_read_failures;
static uint32_t notify_failures;

static uint32_t period_ms_from_odr(uint16_t hz)
{
    switch (hz) {
    case 20:
        return 50;
    case 50:
        return 20;
    case 100:
        return 10;
    default:
        return 10;
    }
}

static void odr_work_handler(struct k_work *work)
{
    int err;

    ARG_UNUSED(work);

    err = imu_set_odr_hz(pending_imu_odr_hz);
    if (err) {
        LOG_WRN("imu_set_odr_hz failed for %u Hz: %d", pending_imu_odr_hz, err);
        return;
    }

    LOG_INF("IMU ODR applied asynchronously: %u Hz", pending_imu_odr_hz);
}

static void stream_work_handler(struct k_work *work)
{
    struct imu_sample sample;
    uint8_t packet[IMU_PACKET_SIZE_BYTES];
    int err;

    ARG_UNUSED(work);

    if (!running) {
        return;
    }

    err = imu_read(&sample);
    if (err) {
        packets_dropped++;
        imu_read_failures++;
        LOG_WRN("imu_read failed: %d", err);
        return;
    }

    imu_packet_build_v1(
        packet,
        IMU_PACKET_FLAG_STREAMING,
        seq++,
        timebase_uptime_us32(),
        sample.accel_x,
        sample.accel_y,
        sample.accel_z,
        sample.gyro_x,
        sample.gyro_y,
        sample.gyro_z
    );

    err = imu_measurement_notify(packet);
    if (err) {
        packets_dropped++;
        notify_failures++;

        /*
         * This is expected if no central is connected or notifications
         * are not enabled yet, so keep it quiet.
         */
        if (err != -ENOTCONN) {
            LOG_WRN("imu_measurement_notify failed: %d", err);
        }

        return;
    }

    packets_sent++;
}

static void stream_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    k_work_submit(&stream_work);
}

void streamer_init(void)
{
    running = false;
    odr_hz = 100;
    seq = 0;
    packets_sent = 0;
    packets_dropped = 0;
    imu_read_failures = 0;
    notify_failures = 0;

    pending_imu_odr_hz = odr_hz;

    k_work_init(&stream_work, stream_work_handler);
    k_work_init(&odr_work, odr_work_handler);
    k_timer_init(&stream_timer, stream_timer_handler, NULL);

    LOG_INF("Streamer initialized");
}

int streamer_set_odr_hz(uint16_t new_odr_hz)
{
    uint32_t period_ms;

    switch (new_odr_hz) {
    case 20:
    case 50:
    case 100:
        break;
    default:
        LOG_WRN("Unsupported streamer ODR: %u", new_odr_hz);
        return -EINVAL;
    }

    odr_hz = new_odr_hz;
    pending_imu_odr_hz = new_odr_hz;

    /*
    * Apply physical IMU ODR outside the GATT write callback path.
    * This keeps SET_ODR fast when multiple sensors are being configured.
    */
    k_work_submit(&odr_work);

    if (running) {
        period_ms = period_ms_from_odr(odr_hz);
        k_timer_start(&stream_timer, K_MSEC(period_ms), K_MSEC(period_ms));
    }

    LOG_INF("Streamer ODR set to %u Hz", odr_hz);

    return 0;
}

uint16_t streamer_get_odr_hz(void)
{
    return odr_hz;
}

void streamer_start(void)
{
    uint32_t period_ms;

    if (running) {
        return;
    }

    seq = 0;
    packets_sent = 0;
    packets_dropped = 0;
    imu_read_failures = 0;
    notify_failures = 0;

    running = true;

    period_ms = period_ms_from_odr(odr_hz);
    k_timer_start(&stream_timer, K_MSEC(period_ms), K_MSEC(period_ms));

    status_led_set_mode(STATUS_LED_MODE_STREAMING);

    LOG_INF("Streaming started at %u Hz", odr_hz);
}

void streamer_stop(void)
{
    if (!running) {
        return;
    }

    running = false;
    k_timer_stop(&stream_timer);

    status_led_set_mode(STATUS_LED_MODE_CONNECTED);

    LOG_INF("Streaming stopped");
}

bool streamer_is_running(void)
{
    return running;
}

uint32_t streamer_get_packets_sent(void)
{
    return packets_sent;
}

uint32_t streamer_get_packets_dropped(void)
{
    return packets_dropped;
}

uint32_t streamer_get_imu_read_failures(void)
{
    return imu_read_failures;
}

uint32_t streamer_get_notify_failures(void)
{
    return notify_failures;
}
