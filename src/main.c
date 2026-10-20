#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "button_manager.h"
#include "display_manager.h"
#include "vibration_manager.h"
#if defined(CONFIG_NEXUS_DUMMY)
#include "dummy_nexus_client.h"
#else
#include "nexus_client.h"
#endif
#include "hud_state_machine.h"
#include "wifi_manager.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static void handle_button_press(void)
{
    hud_state_machine_handle_single_press();
}

#if defined(CONFIG_NEXUS_DUMMY)
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

#endif

static void handle_wifi_state(enum wifi_manager_state state)
{
    const struct hud_event event = {
        .type = HUD_EVENT_WIFI,
        .connection = state == WIFI_MANAGER_READY ? HUD_CONNECTED :
            (state == WIFI_MANAGER_CONNECTING || state == WIFI_MANAGER_ASSOCIATED)
                ? HUD_CONNECTING : HUD_DISCONNECTED,
    };
    hud_state_machine_handle_event(&event);
#if !defined(CONFIG_NEXUS_DUMMY)
    nexus_client_wifi_changed();
#endif
}

int main(void)
{
    LOG_INF("Starting Nexus AR HUD firmware 0.1.0");

    int error = display_manager_init();

    if (error != 0) {
        LOG_ERR("Display initialization failed: %d", error);
        return error;
    }

    error = hud_state_machine_init(IS_ENABLED(CONFIG_NEXUS_DUMMY), NULL);

    if (error != 0) {
        LOG_ERR("HUD state machine initialization failed: %d", error);
        return error;
    }

    error = vibration_manager_init();
    if (error) {
        LOG_ERR("Vibration initialization failed: %d", error);
        return error;
    }

    error = button_manager_init(handle_button_press, hud_state_machine_handle_double_press);

    if (error != 0) {
        LOG_ERR("Button initialization failed: %d", error);
        return error;
    }

#if defined(CONFIG_NEXUS_DUMMY)
    error = dummy_nexus_client_start(handle_nexus_event);
#else
    error = nexus_client_start();
#endif

    if (error != 0) {
        LOG_ERR("Nexus event client failed to start: %d", error);
        return error;
    }

    LOG_INF("Nexus AR HUD initialized");

    error = wifi_manager_init(handle_wifi_state);

    if (error != 0) {
        LOG_ERR("Wi-Fi initialization failed: %d", error);
    } else {
        /*
         * Preserve the existing driver settling delay.
         * The display, controller and event client are already running.
         */
        k_sleep(K_SECONDS(3));

        error = wifi_manager_connect();

        if (error != 0) {
            LOG_WRN("Wi-Fi connection request failed: %d", error);
        } else {
            LOG_INF("Wi-Fi connection requested");
        }
    }

    while (true) {
        k_sleep(K_FOREVER);
    }

    return 0;
}