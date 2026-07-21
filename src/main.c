#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "button_manager.h"
#include "display_manager.h"
#include "dummy_nexus_client.h"
#include "hud_state_machine.h"
#include "vibration_manager.h"
#include "wifi_manager.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static void handle_button_press(void)
{
    /*
     * This currently does nothing because the state-machine
     * single-press behavior has not yet been implemented.
     */
    hud_state_machine_handle_single_press();
}

static void handle_nexus_event(
    const struct hud_event *event
)
{
    int error = hud_state_machine_handle_event(event);

    if (error != 0) {
        LOG_ERR(
            "Failed to handle dummy Nexus event: %d",
            error
        );
    }
}

int main(void)
{
    LOG_INF("Starting Nexus AR HUD firmware 0.1.0");

    int error = wifi_manager_init();

    if (error != 0 ){
        LOG_ERR(
            "Wifi manager initialization failed: %d",
            error
        );
        return error;
    }

    LOG_INF("Waiting for Wi-Fi driver initialization");
    k_sleep(K_SECONDS(3));

    error = wifi_manager_connect();

    if (error != 0) {
        LOG_ERR("Wi-Fi connection request failed: %d", error);
        return error;
    }

    error = wifi_manager_wait_ready(K_SECONDS(40));

    if (error != 0) {
        LOG_ERR("Wi-Fi did not become ready: %d", error);
        return error;
    }

    LOG_INF("Wi-Fi is ready");

    error = vibration_manager_init();

    if (error != 0) {
        LOG_ERR(
            "Vibration manager initialization failed: %d",
            error
        );

        return error;
    }

    error = display_manager_init();

    if (error != 0) {
        LOG_ERR(
            "Display manager initialization failed: %d",
            error
        );

        return error;
    }

    error = hud_state_machine_init(
        dummy_nexus_client_send_command
    );

    if (error != 0) {
        LOG_ERR(
            "HUD state machine initialization failed: %d",
            error
        );

        return error;
    }

    error = button_manager_init(
        handle_button_press
    );

    if (error != 0) {
        LOG_ERR(
            "Button manager initialization failed: %d",
            error
        );

        return error;
    }

    error = dummy_nexus_client_start(
        handle_nexus_event
    );

    if (error != 0) {
        LOG_ERR(
            "Dummy Nexus client failed to start: %d",
            error
        );

        return error;
    }

    LOG_INF("Nexus AR HUD initialized");

    while (true) {
        k_sleep(K_FOREVER);
    }

    return 0;
}