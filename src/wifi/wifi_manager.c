#include "wifi_manager.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(wifi_manager, CONFIG_WIFI_MANAGER_LOG_LEVEL);

#define WIFI_EVENT_MASK                                                   \
    (NET_EVENT_WIFI_CONNECT_RESULT |                                      \
     NET_EVENT_WIFI_DISCONNECT_RESULT |                                   \
     NET_EVENT_IPV4_ADDR_ADD)

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_event_callback;

static atomic_t wifi_state =
    ATOMIC_INIT(WIFI_MANAGER_DISCONNECTED);

K_SEM_DEFINE(wifi_ready_sem, 0, 1);


// sets the current state of the wifi manager and updates the atomic variable
static void set_state(enum wifi_manager_state state)
{
    atomic_set(&wifi_state, state);
}

//
static void log_ipv4_address(struct net_if *iface)
{
    char address_buffer[NET_IPV4_ADDR_LEN];

    for (int index = 0;
         index < NET_IF_MAX_IPV4_ADDR;
         ++index) {

        const struct net_if_addr_ipv4 *if_addr =
            &iface->config.ip.ipv4->unicast[index];

        if (!if_addr->ipv4.is_used) {
            continue;
        }

        if (if_addr->ipv4.address.family != AF_INET) {
            continue;
        }

        const char *address = net_addr_ntop(
            AF_INET,
            &if_addr->ipv4.address.in_addr,
            address_buffer,
            sizeof(address_buffer)
        );

        if (address != NULL) {
            LOG_INF("IPv4 address: %s", address);
            return;
        }
    }

    LOG_WRN("IPv4 event received but no address was found");
}


//handles events procduced during wifi management process
static void wifi_event_handler(
    struct net_mgmt_event_callback *callback,
    uint64_t event,
    struct net_if *iface
)
{
    if (iface != wifi_iface) {
        return;
    }

    switch (event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status =
            callback->info;

        if (status == NULL) {
            LOG_ERR("Wi-Fi result contained no status");
            set_state(WIFI_MANAGER_ERROR);
            return;
        }

        if (status->status != 0) {
            LOG_ERR(
                "Wi-Fi association failed: %d",
                status->status
            );

            set_state(WIFI_MANAGER_ERROR);
            return;
        }

        LOG_INF("Wi-Fi associated; waiting for DHCP");
        set_state(WIFI_MANAGER_ASSOCIATED);
        break;
    }

    case NET_EVENT_WIFI_DISCONNECT_RESULT: {
        const struct wifi_status *status =
            callback->info;

        int reason =
            status != NULL ? status->status : 0;

        LOG_WRN("Wi-Fi disconnected: %d", reason);

        set_state(WIFI_MANAGER_DISCONNECTED);
        k_sem_reset(&wifi_ready_sem);
        break;
    }

    case NET_EVENT_IPV4_ADDR_ADD:
        LOG_INF("Network is ready");

        log_ipv4_address(iface);

        set_state(WIFI_MANAGER_READY);
        k_sem_give(&wifi_ready_sem);
        break;

    default:
        break;
    }
}

int wifi_manager_init(void)
{
    wifi_iface = net_if_get_default();

    if (wifi_iface == NULL) {
        LOG_ERR("No default network interface found");
        return -ENODEV;
    }

    net_mgmt_init_event_callback(
        &wifi_event_callback,
        wifi_event_handler,
        WIFI_EVENT_MASK
    );

    net_mgmt_add_event_callback(
        &wifi_event_callback
    );

    set_state(WIFI_MANAGER_DISCONNECTED);
    k_sem_reset(&wifi_ready_sem);

    LOG_INF("Wi-Fi manager initialized");

    return 0;
}

int wifi_manager_connect(void)
{
    if (wifi_iface == NULL) {
        return -ENODEV;
    }

    static struct wifi_connect_req_params connection_params;

    memset(
        &connection_params,
        0,
        sizeof(connection_params)
    );

    connection_params.ssid =
        (const uint8_t *)CONFIG_WIFI_MANAGER_SSID;

    connection_params.ssid_length =
        strlen(CONFIG_WIFI_MANAGER_SSID);

    if (IS_ENABLED(CONFIG_WIFI_MANAGER_OPEN_NETWORK)) {
        connection_params.security =
            WIFI_SECURITY_TYPE_NONE;
    } else {
        connection_params.psk =
            (const uint8_t *)CONFIG_WIFI_MANAGER_PSK;

        connection_params.psk_length =
            strlen(CONFIG_WIFI_MANAGER_PSK);

        connection_params.security =
            WIFI_SECURITY_TYPE_PSK;
    }

    connection_params.band =
        WIFI_FREQ_BAND_2_4_GHZ;

    connection_params.channel =
        WIFI_CHANNEL_ANY;

    connection_params.mfp =
        WIFI_MFP_OPTIONAL;

    LOG_INF(
        "Connecting to SSID: %s",
        CONFIG_WIFI_MANAGER_SSID
    );

    set_state(WIFI_MANAGER_CONNECTING);
    k_sem_reset(&wifi_ready_sem);

    int error = net_mgmt(
        NET_REQUEST_WIFI_CONNECT,
        wifi_iface,
        &connection_params,
        sizeof(connection_params)
    );

    if (error != 0) {
        LOG_ERR(
            "Wi-Fi request failed: %d",
            error
        );

        set_state(WIFI_MANAGER_ERROR);
        return error;
    }

    return 0;
}

bool wifi_manager_is_ready(void)
{
    return atomic_get(&wifi_state) ==
           WIFI_MANAGER_READY;
}

enum wifi_manager_state wifi_manager_get_state(void)
{
    return (enum wifi_manager_state)
        atomic_get(&wifi_state);
}

int wifi_manager_wait_ready(k_timeout_t timeout)
{
    if (wifi_manager_is_ready()) {
        return 0;
    }

    int error = k_sem_take(
        &wifi_ready_sem,
        timeout
    );

    if (error != 0) {
        return -ETIMEDOUT;
    }

    return wifi_manager_is_ready()
        ? 0
        : -ENETDOWN;
}