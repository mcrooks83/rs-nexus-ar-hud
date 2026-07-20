#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <zephyr/kernel.h>

enum wifi_manager_state {
    WIFI_MANAGER_DISCONNECTED,
    WIFI_MANAGER_CONNECTING,
    WIFI_MANAGER_ASSOCIATED,
    WIFI_MANAGER_READY,
    WIFI_MANAGER_ERROR,
};

int wifi_manager_init(void);
int wifi_manager_connect(void);

bool wifi_manager_is_ready(void);
enum wifi_manager_state wifi_manager_get_state(void);

int wifi_manager_wait_ready(k_timeout_t timeout);

#endif