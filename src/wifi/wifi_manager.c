#include "wifi_manager.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(wifi_manager, CONFIG_WIFI_MANAGER_LOG_LEVEL);

#define WIFI_EVENT_MASK                                                   \
    (NET_EVENT_WIFI_CONNECT_RESULT |                                      \
     NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_EVENT_MASK NET_EVENT_IPV4_ADDR_ADD

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_event_callback;
static struct net_mgmt_event_callback ipv4_event_callback;

static atomic_t wifi_state =
    ATOMIC_INIT(WIFI_MANAGER_DISCONNECTED);

K_SEM_DEFINE(wifi_ready_sem, 0, 1);

static const char *state_to_string(
    enum wifi_manager_state state
)
{
    switch (state) {
    case WIFI_MANAGER_DISCONNECTED:
        return "disconnected";
    case WIFI_MANAGER_CONNECTING:
        return "connecting";
    case WIFI_MANAGER_ASSOCIATED:
        return "associated";
    case WIFI_MANAGER_READY:
        return "ready";
    case WIFI_MANAGER_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static void signal_waiters(void)
{
    if (k_sem_count_get(&wifi_ready_sem) == 0) {
        k_sem_give(&wifi_ready_sem);
    }
}

static void set_state(enum wifi_manager_state state)
{
    atomic_set(&wifi_state, state);
}

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

static void log_iface_status(void)
{
    if (wifi_iface == NULL) {
        LOG_WRN("Wi-Fi status unavailable: no interface");
        return;
    }

    struct wifi_iface_status status;
    char ssid[WIFI_SSID_MAX_LEN + 1];

    memset(&status, 0, sizeof(status));
    memset(ssid, 0, sizeof(ssid));

    int error = net_mgmt(
        NET_REQUEST_WIFI_IFACE_STATUS,
        wifi_iface,
        &status,
        sizeof(status)
    );

    if (error != 0) {
        LOG_WRN(
            "Failed to query Wi-Fi interface status: %d",
            error
        );
        return;
    }

    size_t ssid_length = status.ssid_len;

    if (ssid_length > WIFI_SSID_MAX_LEN) {
        ssid_length = WIFI_SSID_MAX_LEN;
    }

    if (ssid_length > 0) {
        memcpy(ssid, status.ssid, ssid_length);
    } else {
        strcpy(ssid, "<none>");
    }

    LOG_INF(
        "Wi-Fi iface state=%s(%d) ssid=%s channel=%u rssi=%d security=%d mfp=%d",
        wifi_state_txt(status.state),
        status.state,
        ssid,
        status.channel,
        status.rssi,
        status.security,
        status.mfp
    );
}

static void log_link_status(void)
{
    if (wifi_iface == NULL) {
        LOG_WRN("Wi-Fi link status unavailable: no interface");
        return;
    }

    LOG_INF(
        "Wi-Fi iface admin_up=%d oper_up=%d carrier_ok=%d dormant=%d",
        net_if_is_admin_up(wifi_iface),
        net_if_is_up(wifi_iface),
        net_if_is_carrier_ok(wifi_iface),
        net_if_is_dormant(wifi_iface)
    );
}

static void wifi_event_handler(
    struct net_mgmt_event_callback *callback,
    uint64_t event,
    struct net_if *iface
)
{
    LOG_INF(
        "Wi-Fi event 0x%llx iface=%p expected=%p",
        (unsigned long long)event,
        iface,
        wifi_iface
    );

    if (iface != wifi_iface) {
        LOG_WRN("Ignoring Wi-Fi event from unexpected interface");
        return;
    }

    switch (event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status = callback->info;

        if (status == NULL) {
            LOG_ERR("Wi-Fi connect result contained no status");
            set_state(WIFI_MANAGER_ERROR);
            signal_waiters();
            return;
        }

        if (status->conn_status != WIFI_STATUS_CONN_SUCCESS) {
            LOG_ERR(
                "Wi-Fi connection failed: %s (%d)",
                wifi_conn_status_txt(status->conn_status),
                status->conn_status
            );

            set_state(WIFI_MANAGER_ERROR);
            log_link_status();
            log_iface_status();
            signal_waiters();
            return;
        }

                /*
        * Do not regress READY back to ASSOCIATED.
        *
        * On the ESP32 driver with automatic DHCP enabled, the successful
        * connect-result event may be raised after the IPv4 address event.
        */
        if (atomic_cas(
                &wifi_state,
                WIFI_MANAGER_CONNECTING,
                WIFI_MANAGER_ASSOCIATED)) {

            LOG_INF("Wi-Fi associated; waiting for DHCP");
        } else if (wifi_manager_get_state() == WIFI_MANAGER_READY) {
            LOG_INF("Wi-Fi connection confirmed; network already ready");
        } else {
            LOG_WRN(
                "Wi-Fi connected while manager state=%s",
                state_to_string(wifi_manager_get_state())
            );
        }

        break;
    }

    case NET_EVENT_WIFI_DISCONNECT_RESULT: {
        const struct wifi_status *status = callback->info;
        int reason = status != NULL ? status->disconn_reason : 0;

        LOG_WRN("Wi-Fi disconnected: reason=%d", reason);

        set_state(WIFI_MANAGER_DISCONNECTED);
        signal_waiters();
        break;
    }

    default:
        break;
    }
}

static void ipv4_event_handler(
    struct net_mgmt_event_callback *callback,
    uint64_t event,
    struct net_if *iface
)
{
    ARG_UNUSED(callback);

    if (event != NET_EVENT_IPV4_ADDR_ADD) {
        return;
    }

    if (iface != wifi_iface) {
        return;
    }

    LOG_INF("Network is ready");
    log_ipv4_address(iface);
    set_state(WIFI_MANAGER_READY);
    signal_waiters();
}

int wifi_manager_init(void)
{
    wifi_iface = net_if_get_wifi_sta();

    if (wifi_iface == NULL) {
        wifi_iface = net_if_get_first_wifi();
    }

    if (wifi_iface == NULL) {
        wifi_iface = net_if_get_default();
    }

    if (wifi_iface == NULL) {
        LOG_ERR("No network interface found");
        return -ENODEV;
    }

    net_mgmt_init_event_callback(
        &wifi_event_callback,
        wifi_event_handler,
        WIFI_EVENT_MASK
    );

    net_mgmt_init_event_callback(
        &ipv4_event_callback,
        ipv4_event_handler,
        IPV4_EVENT_MASK
    );

    net_mgmt_add_event_callback(&wifi_event_callback);
    net_mgmt_add_event_callback(&ipv4_event_callback);

    set_state(WIFI_MANAGER_DISCONNECTED);
    k_sem_reset(&wifi_ready_sem);

    LOG_INF("Wi-Fi manager initialized");
    log_link_status();
    log_iface_status();

    return 0;
}

int wifi_manager_connect(void)
{
    if (wifi_iface == NULL) {
        return -ENODEV;
    }

    static struct wifi_connect_req_params connection_params;
    const size_t ssid_length = strlen(CONFIG_WIFI_MANAGER_SSID);
    size_t psk_length = 0;
    int error;

    if (ssid_length == 0 || ssid_length > WIFI_SSID_MAX_LEN) {
        LOG_ERR(
            "Invalid Wi-Fi SSID length: %u",
            (unsigned int)ssid_length
        );
        return -EINVAL;
    }

    if (!IS_ENABLED(CONFIG_WIFI_MANAGER_OPEN_NETWORK)) {
        psk_length = strlen(CONFIG_WIFI_MANAGER_PSK);

        if (psk_length < WIFI_PSK_MIN_LEN ||
            psk_length > WIFI_PSK_MAX_LEN) {
            LOG_ERR(
                "Invalid Wi-Fi PSK length: %u",
                (unsigned int)psk_length
            );
            return -EINVAL;
        }
    }

    memset(&connection_params, 0, sizeof(connection_params));

    connection_params.ssid =
        (const uint8_t *)CONFIG_WIFI_MANAGER_SSID;
    connection_params.ssid_length = (uint8_t)ssid_length;

    if (IS_ENABLED(CONFIG_WIFI_MANAGER_OPEN_NETWORK)) {
        connection_params.security = WIFI_SECURITY_TYPE_NONE;
    } else {
        connection_params.psk =
            (const uint8_t *)CONFIG_WIFI_MANAGER_PSK;
        connection_params.psk_length = (uint8_t)psk_length;
        connection_params.security = WIFI_SECURITY_TYPE_PSK;
    }

    connection_params.band = WIFI_FREQ_BAND_2_4_GHZ;
    connection_params.channel = WIFI_CHANNEL_ANY;

    /* The Nexus N3 AP is WPA2-PSK and does not require 802.11w PMF. */
    connection_params.mfp = WIFI_MFP_DISABLE;

    /* Zephyr defines this field in seconds. */
    connection_params.timeout = 30;

    LOG_INF(
        "Connecting to SSID: %s",
        CONFIG_WIFI_MANAGER_SSID
    );

    LOG_INF(
        "Wi-Fi params ssid_len=%u psk_len=%u security=%d mfp=%d band=%d channel=%u timeout=%d",
        connection_params.ssid_length,
        connection_params.psk_length,
        connection_params.security,
        connection_params.mfp,
        connection_params.band,
        connection_params.channel,
        connection_params.timeout
    );

    set_state(WIFI_MANAGER_CONNECTING);
    k_sem_reset(&wifi_ready_sem);

    error = net_if_up(wifi_iface);

    if (error == -EALREADY) {
        LOG_INF("Wi-Fi interface was already up");
    } else {
        LOG_INF("net_if_up returned: %d", error);
    }

    log_link_status();

    if (error != 0 && error != -EALREADY) {
        set_state(WIFI_MANAGER_ERROR);
        return error;
    }

    error = net_mgmt(
        NET_REQUEST_WIFI_CONNECT,
        wifi_iface,
        &connection_params,
        sizeof(connection_params)
    );

    if (error != 0) {
        LOG_ERR("Wi-Fi request failed: %d", error);
        set_state(WIFI_MANAGER_ERROR);
        return error;
    }

    LOG_INF("Wi-Fi connect request accepted");

    return 0;
}

bool wifi_manager_is_ready(void)
{
    return atomic_get(&wifi_state) == WIFI_MANAGER_READY;
}

enum wifi_manager_state wifi_manager_get_state(void)
{
    return (enum wifi_manager_state)atomic_get(&wifi_state);
}

int wifi_manager_wait_ready(k_timeout_t timeout)
{
    if (wifi_manager_is_ready()) {
        return 0;
    }

    int error = k_sem_take(&wifi_ready_sem, timeout);

    if (error != 0) {
        enum wifi_manager_state state = wifi_manager_get_state();

        LOG_ERR(
            "Timed out waiting for Wi-Fi readiness; manager state=%s",
            state_to_string(state)
        );
        log_link_status();
        log_iface_status();

        return -ETIMEDOUT;
    }

    switch (wifi_manager_get_state()) {
    case WIFI_MANAGER_READY:
        return 0;
    case WIFI_MANAGER_ERROR:
        log_link_status();
        log_iface_status();
        return -EIO;
    case WIFI_MANAGER_DISCONNECTED:
        log_link_status();
        log_iface_status();
        return -ENETDOWN;
    case WIFI_MANAGER_ASSOCIATED:
    case WIFI_MANAGER_CONNECTING:
    default:
        log_link_status();
        log_iface_status();
        return -ENETUNREACH;
    }
}