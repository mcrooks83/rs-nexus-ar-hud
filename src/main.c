#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "button_manager.h"
#include "display_manager.h"
#include "vibration_manager.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static void handle_button_press(void)
{
    int err = vibration_manager_pulse(500);

    if (err != 0) {
        LOG_ERR("Vibration pulse failed: %d", err);
    }
}

int main(void)
{
    LOG_INF("Starting Nexus AR HUD firmware 0.1.0");

    int err = vibration_manager_init();

    if (err != 0) {
        LOG_ERR(
            "Vibration motor init failed: %d",
            err
        );

        return err;
    }

    err = button_manager_init(handle_button_press);

    if (err != 0) {
        LOG_ERR("Button init failed: %d", err);
        return err;
    }

    err = display_manager_init();

    if (err != 0) {
        LOG_ERR(
            "Display initialization failed: %d",
            err
        );

        return err;
    }

    // test the scrolling display
    err = display_manager_start_marquee(
        "NEXUS N3 AR HUD"
    );

    if (err != 0) {
        LOG_ERR(
            "Failed to start display marquee: %d",
            err
        );

        return err;
    }

    LOG_INF("Press the button to trigger vibration");

    // forever loop for testing.
    while (true) {
        k_sleep(K_FOREVER);
    }

    return 0;
}