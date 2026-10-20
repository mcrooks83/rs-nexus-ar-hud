#include "wifi_manager.h"
#include "wifi_retry.h"

#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(wifi_manager, CONFIG_WIFI_MANAGER_LOG_LEVEL);
BUILD_ASSERT(!IS_ENABLED(CONFIG_ESP32_WIFI_STA_RECONNECT),
             "The HUD Wi-Fi worker must be the only reconnect owner");

#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)
#define IP_EVENTS (NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL)
static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_events, ip_events, iface_events;
static wifi_manager_callback_t state_callback;
static struct wifi_connect_req_params connection_params;
static struct k_thread wifi_thread;
static K_THREAD_STACK_DEFINE(wifi_stack, 3072);
static atomic_t initialized, enabled, failed_or_lost;
static atomic_t wifi_state = ATOMIC_INIT(WIFI_MANAGER_DISCONNECTED);
K_SEM_DEFINE(wifi_wake, 0, 1);
K_SEM_DEFINE(wifi_ready_sem, 0, 1);

/* Network callbacks only copy the failure signal and wake the worker. They
 * retain no callback->info pointer and never request a connection or draw. */
static void network_event(struct net_mgmt_event_callback *callback,
                          uint64_t event, struct net_if *iface)
{
    if (iface != wifi_iface) return;
    if (event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const struct wifi_status *status = callback->info;
        if (!status || status->conn_status != WIFI_STATUS_CONN_SUCCESS)
            atomic_set(&failed_or_lost, 1);
    } else if (event == NET_EVENT_WIFI_DISCONNECT_RESULT || event == NET_EVENT_IF_DOWN) {
        atomic_set(&failed_or_lost, 1);
    } else if (event != NET_EVENT_IPV4_ADDR_ADD && event != NET_EVENT_IPV4_ADDR_DEL) {
        return;
    }
    k_sem_give(&wifi_wake);
}

static void publish_state(enum wifi_manager_state state)
{
    if (atomic_get(&wifi_state) == (int)state) return;
    atomic_set(&wifi_state, state);
    LOG_INF("Wi-Fi state: %d", state);
    if (state_callback) state_callback(state);
    if (state == WIFI_MANAGER_READY || state == WIFI_MANAGER_DISCONNECTED)
        k_sem_give(&wifi_ready_sem);
}

static enum wifi_link read_link(void)
{
    struct wifi_iface_status status = {0};
    int err = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, wifi_iface, &status, sizeof(status));
    if (err) {
        LOG_WRN("Cannot read Wi-Fi driver state: %d", err);
        return WIFI_LINK_UNKNOWN;
    }
    switch (status.state) {
    case WIFI_STATE_INACTIVE:
    case WIFI_STATE_DISCONNECTED:
    case WIFI_STATE_INTERFACE_DISABLED:
        return WIFI_LINK_IDLE;
    case WIFI_STATE_COMPLETED:
        /* An old DHCP address alone is not evidence of a live association. */
        return net_if_ipv4_get_global_addr(wifi_iface, NET_ADDR_PREFERRED)
            ? WIFI_LINK_IP : WIFI_LINK_ASSOCIATED;
    default:
        return WIFI_LINK_BUSY;
    }
}

static int start_attempt(void)
{
    int err = net_if_up(wifi_iface);
    if (err && err != -EALREADY) return err;
    return net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
                    &connection_params, sizeof(connection_params));
}

static void cancel_attempt(void)
{
    /* Clear stale leases and stop DHCP even if association never reported
     * success. The ESP32 driver starts DHCP again on the next association. */
    net_dhcpv4_stop(wifi_iface);
    int err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
    if (err) LOG_DBG("Disconnect request returned %d; waiting for driver idle", err);
}

/* One bounded worker iteration; driver operations are confined here. */
static int64_t worker_step(struct wifi_retry *retry)
{
    bool failed = atomic_set(&failed_or_lost, 0) != 0;
    enum wifi_link link = read_link();
    int64_t now = k_uptime_get();
    enum wifi_retry_phase previous = retry->phase;
    enum wifi_retry_action action = wifi_retry_step(retry, now, link, failed);
    enum wifi_manager_state state = WIFI_MANAGER_DISCONNECTED;
    if (retry->phase == WIFI_RETRY_READY) state = WIFI_MANAGER_READY;
    else if (retry->phase == WIFI_RETRY_ATTEMPT)
        state = link == WIFI_LINK_ASSOCIATED ? WIFI_MANAGER_ASSOCIATED : WIFI_MANAGER_CONNECTING;
    publish_state(state);
    if (previous == WIFI_RETRY_CANCEL && retry->phase == WIFI_RETRY_WAIT)
        LOG_INF("Wi-Fi retry in %lld ms", (long long)(retry->deadline - now));
    if (action == WIFI_RETRY_CONNECT) {
        k_sem_reset(&wifi_ready_sem);
        LOG_INF("Requesting Wi-Fi connection (45 s IP deadline)");
        int err = start_attempt();
        if (err) {
            LOG_WRN("Wi-Fi connection request failed: %d", err);
            atomic_set(&failed_or_lost, 1);
            return 0;
        }
    } else if (action == WIFI_RETRY_DISCONNECT) {
        if (previous == WIFI_RETRY_ATTEMPT && !failed)
            LOG_WRN("Wi-Fi IP deadline expired; cancelling attempt");
        cancel_attempt();
    }
    int64_t delay = retry->phase == WIFI_RETRY_CANCEL ? 250 : 1000;
    if (retry->phase != WIFI_RETRY_READY)
        delay = MIN(delay, MAX(1, retry->deadline - k_uptime_get()));
    return delay;
}

static void wifi_worker(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    /* main enables this only after display/dummy startup and settling delay. */
    while (!atomic_get(&enabled)) k_sem_take(&wifi_wake, K_FOREVER);
    struct wifi_retry retry;
    wifi_retry_init(&retry, k_uptime_get());
    while (true) {
        int64_t delay = worker_step(&retry);
        /* Events wake this early. Polling also observes IP loss and the
         * driver's asynchronous cancellation without starting a new attempt. */
        k_sem_take(&wifi_wake, K_MSEC(delay));
    }
}

static int prepare_connection(void)
{
    size_t ssid_len = strlen(CONFIG_WIFI_MANAGER_SSID);
    size_t psk_len = strlen(CONFIG_WIFI_MANAGER_PSK);
    if (!ssid_len || ssid_len > WIFI_SSID_MAX_LEN) return -EINVAL;
    if (!IS_ENABLED(CONFIG_WIFI_MANAGER_OPEN_NETWORK) &&
        (psk_len < WIFI_PSK_MIN_LEN || psk_len > WIFI_PSK_MAX_LEN)) return -EINVAL;
    connection_params = (struct wifi_connect_req_params) {
        .ssid = (const uint8_t *)CONFIG_WIFI_MANAGER_SSID,
        .ssid_length = ssid_len,
        .security = WIFI_SECURITY_TYPE_NONE,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .channel = WIFI_CHANNEL_ANY,
        .mfp = WIFI_MFP_DISABLE,
        .timeout = 30,
    };
    if (!IS_ENABLED(CONFIG_WIFI_MANAGER_OPEN_NETWORK)) {
        connection_params.security = WIFI_SECURITY_TYPE_PSK;
        connection_params.psk = (const uint8_t *)CONFIG_WIFI_MANAGER_PSK;
        connection_params.psk_length = psk_len;
    }
    return 0;
}

int wifi_manager_init(wifi_manager_callback_t callback)
{
    /* Claim before registering callbacks/creating the single worker. */
    if (!atomic_cas(&initialized, 0, 1)) return -EALREADY;
    int err = prepare_connection();
    if (err) goto fail;
    wifi_iface = net_if_get_wifi_sta();
    if (!wifi_iface) wifi_iface = net_if_get_first_wifi();
    if (!wifi_iface) { err = -ENODEV; goto fail; }
    state_callback = callback;
    net_mgmt_init_event_callback(&wifi_events, network_event, WIFI_EVENTS);
    net_mgmt_init_event_callback(&ip_events, network_event, IP_EVENTS);
    net_mgmt_init_event_callback(&iface_events, network_event, NET_EVENT_IF_DOWN);
    net_mgmt_add_event_callback(&wifi_events);
    net_mgmt_add_event_callback(&ip_events);
    net_mgmt_add_event_callback(&iface_events);
    k_thread_create(&wifi_thread, wifi_stack, K_THREAD_STACK_SIZEOF(wifi_stack),
                    wifi_worker, NULL, NULL, NULL, 7, 0, K_NO_WAIT);
    k_thread_name_set(&wifi_thread, "hud_wifi");
    if (state_callback) state_callback(WIFI_MANAGER_DISCONNECTED);
    atomic_set(&initialized, 2);
    return 0;
fail:
    atomic_set(&initialized, 0);
    return err;
}

int wifi_manager_connect(void)
{
    if (atomic_get(&initialized) != 2) return -ENODEV;
    if (!atomic_cas(&enabled, 0, 1)) return -EALREADY;
    k_sem_give(&wifi_wake);
    return 0;
}

bool wifi_manager_is_ready(void)
{
    return wifi_manager_get_state() == WIFI_MANAGER_READY;
}
enum wifi_manager_state wifi_manager_get_state(void)
{
    return (enum wifi_manager_state)atomic_get(&wifi_state);
}
int wifi_manager_wait_ready(k_timeout_t timeout)
{
    if (wifi_manager_is_ready()) return 0;
    if (k_sem_take(&wifi_ready_sem, timeout)) return -ETIMEDOUT;
    return wifi_manager_is_ready() ? 0 : -ENETDOWN;
}
