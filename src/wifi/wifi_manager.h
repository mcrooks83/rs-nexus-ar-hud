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

typedef void (*wifi_manager_callback_t)(enum wifi_manager_state state);
int wifi_manager_init(wifi_manager_callback_t callback);
/* Enables automatic connection/recovery. Nonblocking; repeated calls return
 * -EALREADY and cannot create another worker or bypass the backoff. */
int wifi_manager_connect(void);

bool wifi_manager_is_ready(void);
enum wifi_manager_state wifi_manager_get_state(void);

int wifi_manager_wait_ready(k_timeout_t timeout);

#endif