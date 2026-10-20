#include <assert.h>
#include <stdio.h>
#define CONFIG_WIFI_MANAGER_LOG_LEVEL 3
#define CONFIG_WIFI_MANAGER_SSID "test-ap"
#define CONFIG_WIFI_MANAGER_PSK "test-passphrase"
#define CONFIG_WIFI_MANAGER_OPEN_NETWORK 0
#define CONFIG_ESP32_WIFI_STA_RECONNECT 0
#include "../src/wifi/wifi_manager.c"

int64_t test_now;
unsigned test_threads;
static struct net_if iface, other_iface;
static enum wifi_iface_state driver_state = WIFI_STATE_DISCONNECTED;
static bool has_ip, in_callback;
static unsigned connects, disconnects, queries, publications;
static int connect_error, status_error;
struct net_if *net_if_get_wifi_sta(void) { return &iface; }
struct net_if *net_if_get_first_wifi(void) { return &iface; }
void *net_if_ipv4_get_global_addr(struct net_if *i, int state)
{ (void)i; (void)state; assert(!in_callback); return has_ip ? &iface : NULL; }
int net_if_up(struct net_if *i) { (void)i; assert(!in_callback); return 0; }
void net_dhcpv4_stop(struct net_if *i)
{ (void)i; assert(!in_callback); has_ip = false; }
int net_mgmt(int request, struct net_if *i, void *data, size_t size)
{
    (void)i; (void)size; assert(!in_callback);
    if (request == NET_REQUEST_WIFI_IFACE_STATUS) {
        ++queries;
        ((struct wifi_iface_status *)data)->state = driver_state;
        return status_error;
    }
    if (request == NET_REQUEST_WIFI_CONNECT) {
        // Assert against overlapping *driver* attempts, not just model state.
        assert(driver_state == WIFI_STATE_DISCONNECTED || driver_state == WIFI_STATE_INACTIVE);
        ++connects;
        if (connect_error) return connect_error;
        driver_state = WIFI_STATE_SCANNING;
        return 0;
    }
    assert(request == NET_REQUEST_WIFI_DISCONNECT);
    ++disconnects;
    // Cancellation is asynchronous; the test chooses when driver becomes idle.
    return 0;
}
static void published(enum wifi_manager_state state)
{ (void)state; assert(!in_callback); ++publications; }
static void notify(uint64_t event, struct net_if *i, int status)
{
    struct wifi_status data = {.conn_status = status};
    struct net_mgmt_event_callback cb = {.info = &data};
    unsigned old_queries = queries, old_connects = connects, old_publications = publications;
    in_callback = true;
    network_event(&cb, event, i);
    in_callback = false;
    assert(queries == old_queries && connects == old_connects && publications == old_publications);
}
int main(void)
{
    assert(wifi_manager_connect() == -ENODEV);
    assert(wifi_manager_init(published) == 0);
    assert(wifi_manager_init(published) == -EALREADY && test_threads == 1);
    assert(wifi_manager_connect() == 0);
    assert(wifi_manager_connect() == -EALREADY && test_threads == 1);
    struct wifi_retry r;
    wifi_retry_init(&r, test_now);
    // Synchronous initial request failure also schedules retry.
    connect_error = -EIO;
    assert(worker_step(&r) == 0 && connects == 1);
    worker_step(&r);
    assert(wifi_manager_get_state() == WIFI_MANAGER_DISCONNECTED);
    worker_step(&r);
    assert(r.phase == WIFI_RETRY_WAIT && r.deadline == 1000);
    assert(wifi_manager_connect() == -EALREADY);
    test_now = 999; worker_step(&r); assert(connects == 1);
    test_now = 1000; connect_error = 0; worker_step(&r);
    assert(connects == 2 && wifi_manager_get_state() == WIFI_MANAGER_CONNECTING);
    // Association without an IP still blinks and cannot reset backoff.
    driver_state = WIFI_STATE_COMPLETED;
    notify(NET_EVENT_WIFI_CONNECT_RESULT, &iface, WIFI_STATUS_CONN_SUCCESS);
    worker_step(&r);
    assert(wifi_manager_get_state() == WIFI_MANAGER_ASSOCIATED && r.next_delay_ms == 2000);
    test_now = 45999; worker_step(&r); assert(connects == 2);
    test_now = 46000; worker_step(&r);
    assert(r.phase == WIFI_RETRY_CANCEL && wifi_manager_get_state() == WIFI_MANAGER_DISCONNECTED);
    // Late IP from cancelled attempt must never restore READY or reset backoff.
    has_ip = true; notify(NET_EVENT_IPV4_ADDR_ADD, &iface, 0);
    test_now = 46250; worker_step(&r);
    assert(!wifi_manager_is_ready());
    test_now = 47000; worker_step(&r); assert(connects == 2 && !has_ip);
    driver_state = WIFI_STATE_DISCONNECTED;
    notify(NET_EVENT_WIFI_DISCONNECT_RESULT, &iface, 0);
    worker_step(&r); assert(r.phase == WIFI_RETRY_WAIT && r.deadline == 49000);
    test_now = 49000; worker_step(&r); assert(connects == 3);
    // IP can precede the connect result. Late association cannot regress READY.
    driver_state = WIFI_STATE_COMPLETED; has_ip = true;
    notify(NET_EVENT_IPV4_ADDR_ADD, &iface, 0); worker_step(&r);
    assert(wifi_manager_is_ready() && r.next_delay_ms == 1000);
    notify(NET_EVENT_WIFI_CONNECT_RESULT, &iface, WIFI_STATUS_CONN_SUCCESS);
    worker_step(&r); assert(wifi_manager_is_ready());
    notify(NET_EVENT_WIFI_DISCONNECT_RESULT, &other_iface, 0);
    worker_step(&r); assert(wifi_manager_is_ready());
    // AP loss cancels old lease; one second backoff, then restored AP succeeds.
    driver_state = WIFI_STATE_DISCONNECTED;
    notify(NET_EVENT_WIFI_DISCONNECT_RESULT, &iface, 0); worker_step(&r);
    assert(!has_ip && !wifi_manager_is_ready());
    worker_step(&r); assert(r.deadline == 50000);
    test_now = 50000; worker_step(&r); assert(connects == 4);
    driver_state = WIFI_STATE_COMPLETED; has_ip = true;
    notify(NET_EVENT_IPV4_ADDR_ADD, &iface, 0); worker_step(&r);
    assert(wifi_manager_is_ready());
    // IP removal alone must trigger recovery. Unknown driver state cannot connect.
    has_ip = false; notify(NET_EVENT_IPV4_ADDR_DEL, &iface, 0); worker_step(&r);
    assert(!wifi_manager_is_ready());
    status_error = -EIO;
    for (unsigned n = 0; n < 10; ++n) { test_now += 1000; worker_step(&r); }
    assert(connects == 4 && r.phase == WIFI_RETRY_CANCEL);
    assert(test_threads == 1);
    puts("Wi-Fi manager: worker isolation, one worker/attempt, failure, DHCP timeout, indicator states and AP recovery passed");
}
