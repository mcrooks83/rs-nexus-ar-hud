#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_version.h"
#include "ble_services.h"
#include "status_led.h"
#include "imu.h"
#include "streamer.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
    int err;

    LOG_INF("Starting %s firmware %s", APP_DEVICE_NAME, APP_VERSION_STRING);

    err = status_led_init();
    if (err) {
        LOG_WRN("status_led_init failed: %d", err);
    }

    err = imu_init();
    if (err) {
        LOG_ERR("imu_init failed: %d", err);
        return 0;
    }

    status_led_set_mode(STATUS_LED_MODE_DISCOVERY);

    streamer_init();

    err = ble_services_init();
    if (err) {
        LOG_ERR("ble_services_init failed: %d", err);
        return 0;
    }

    LOG_INF("Ready");

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}