#include "nexus_client.h"
#include "nexus_receive.h"
#include "wifi_manager.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>
#include <zephyr/sys/atomic.h>
LOG_MODULE_REGISTER(neia_client, LOG_LEVEL_INF);
BUILD_ASSERT(NEXUS_FINAL == WEBSOCKET_FLAG_FINAL && NEXUS_TEXT == WEBSOCKET_FLAG_TEXT &&
    NEXUS_BINARY == WEBSOCKET_FLAG_BINARY && NEXUS_CLOSE == WEBSOCKET_FLAG_CLOSE &&
    NEXUS_PING == WEBSOCKET_FLAG_PING && NEXUS_PONG == WEBSOCKET_FLAG_PONG);

static struct k_thread thread;
static K_THREAD_STACK_DEFINE(stack, 6144);
static atomic_t started, wifi_epoch;
K_SEM_DEFINE(wake, 0, 1);
/* Dedicated receiver owns all buffers and socket descriptors. */
static struct nexus_parser parser;
static struct nexus_receive rx;
static uint8_t http_buffer[1024];
static void source(enum hud_connection connection)
{
    const struct hud_event event = {.type = HUD_EVENT_SOURCE, .connection = connection};
    hud_state_machine_handle_event(&event);
}
void nexus_client_wifi_changed(void)
{
    atomic_inc(&wifi_epoch);
    k_sem_give(&wake);
}
static bool online(atomic_val_t epoch)
{
    return wifi_manager_is_ready() && atomic_get(&wifi_epoch) == epoch;
}
static int destination(char *host, size_t size)
{
    char gateway[NET_IPV4_ADDR_LEN] = "";
    struct net_if *iface = wifi_manager_get_interface();
    bool ready = wifi_manager_is_ready() && iface &&
        net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (ready) {
        struct net_in_addr gw = net_if_ipv4_get_gw(iface);
        net_addr_ntop(AF_INET, &gw, gateway, sizeof(gateway));
    }
    return nexus_destination(ready, gateway, CONFIG_NEXUS_HOST, host, size);
}
static int connect_tcp(const struct zsock_addrinfo *address, atomic_val_t epoch)
{
    int fd = zsock_socket(address->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -errno;
    int result;
    if (zsock_fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { result = -errno; goto fail; }
    result = zsock_connect(fd, address->ai_addr, address->ai_addrlen);
    if (result && errno != EINPROGRESS) { result = -errno; goto fail; }
    int64_t end = k_uptime_get() + 5000;
    while (result && online(epoch) && k_uptime_get() < end) {
        struct zsock_pollfd pollfd = {.fd = fd, .events = ZSOCK_POLLOUT};
        int count = zsock_poll(&pollfd, 1, 100);
        if (count < 0) { result = -errno; goto fail; }
        if (count) {
            int error = 0; socklen_t len = sizeof(error);
            if (zsock_getsockopt(fd, ZSOCK_SOL_SOCKET, ZSOCK_SO_ERROR, &error, &len)) {
                result = -errno; goto fail;
            }
            if (error) { result = -error; goto fail; }
            result = 0;
        }
    }
    if (!online(epoch)) { result = -ENETDOWN; goto fail; }
    if (result) { result = -ETIMEDOUT; goto fail; }
    if (zsock_fcntl(fd, F_SETFL, 0) < 0) { result = -errno; goto fail; }
    /* Bound close/control sends too, including Zephyr's automatic close reply. */
    struct zsock_timeval timeout = {.tv_sec = 1};
    if (zsock_setsockopt(fd, ZSOCK_SOL_SOCKET, ZSOCK_SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        result = -errno; goto fail;
    }
    return fd;
fail:
    zsock_close(fd);
    return result;
}
static int receive_events(int ws, atomic_val_t epoch)
{
    nexus_receive_reset(&rx);
    uint64_t remaining = 0;
    int64_t partial_since = 0, ping_at = k_uptime_get() + 15000, pong_deadline = 0;
    uint8_t chunk[512];
    while (online(epoch)) {
        int64_t now = k_uptime_get();
        if ((partial_since && now - partial_since > 10000) ||
            (pong_deadline && now > pong_deadline)) return -ETIMEDOUT;
        if (!pong_deadline && now >= ping_at) {
            int err = websocket_send_msg(ws, NULL, 0, WEBSOCKET_OPCODE_PING, true, true, 500);
            if (err < 0) return err;
            pong_deadline = now + 10000;
        }
        /* The installed Zephyr websocket API omits type/remaining outputs
         * when a timeout returns a partial payload. The first byte of a frame
         * establishes its metadata; later partial chunks inherit that type and
         * subtract their byte count from the known frame remainder. */
        size_t capacity = remaining ? MIN(remaining, sizeof(chunk)) : 1;
        uint64_t reported = UINT64_MAX;
        uint32_t type = UINT32_MAX;
        int count = websocket_recv_msg(ws, chunk, capacity, &type, &reported, 500);
        if (count == -EAGAIN) continue;
        if (count < 0) return count;
        if (!online(epoch)) return -ENETDOWN;
        int action = nexus_receive_read(&rx, chunk, count, type, reported, &remaining);
        if (action < 0) return action;
        if ((rx.active || rx.frame_open) && !partial_since) partial_since = now;
        if (!rx.active && !rx.frame_open) partial_since = 0;
        if (action == NEXUS_RX_CLOSE) return -ENOTCONN;
        if (action == NEXUS_RX_PING) {
            int err = websocket_send_msg(ws, rx.control, rx.control_size,
                                         WEBSOCKET_OPCODE_PONG, true, true, 500);
            if (err < 0) return err;
        } else if (action == NEXUS_RX_PONG) {
            pong_deadline = 0; ping_at = now + 15000;
        } else if (action == NEXUS_RX_MESSAGE) {
            struct hud_event event;
            int err = nexus_event_parse(&parser, rx.data, rx.size, &event);
            if (err > 0) {
                if (hud_state_machine_handle_event(&event)) return -ENOBUFS;
            } else if (err == -ENOSPC) {
                LOG_WRN("NEIA event exceeds HUD capacity: one subject, two sensors");
            } else if (err < 0) LOG_WRN("NEIA event rejected: %d (bytes=%u)", err, (unsigned)rx.size);
        }
    }
    return -ENETDOWN;
}
static void run(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    unsigned backoff = 1;
    int64_t retry_at = 0;
    bool had_wifi = false;
    k_sleep(K_SECONDS(1)); /* Leave the boot screen visible before waiting for events. */
    source(HUD_DISCONNECTED);
    while (true) {
        if (!wifi_manager_is_ready()) {
            had_wifi = false;
            k_sem_take(&wake, K_MSEC(500));
            continue;
        }
        if (!had_wifi) { had_wifi = true; backoff = 1; retry_at = 0; }
        if (k_uptime_get() < retry_at) {
            k_sem_take(&wake, K_MSEC(MIN(500, retry_at - k_uptime_get())));
            continue;
        }
        atomic_val_t epoch = atomic_get(&wifi_epoch);
        char host[NEXUS_HOST_MAX], port[6], authority[NEXUS_HOST_MAX + 8];
        int err = destination(host, sizeof(host));
        int tcp = -1, ws = -1;
        struct zsock_addrinfo *address = NULL;
        if (!err) {
            snprintf(port, sizeof(port), "%u", CONFIG_NEXUS_PORT);
            snprintf(authority, sizeof(authority), "%s:%s", host, port);
            LOG_INF("NEIA destination ws://%s%s", authority, CONFIG_NEXUS_PATH);
            source(HUD_CONNECTING);
            struct zsock_addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
            err = zsock_getaddrinfo(host, port, &hints, &address);
            if (!err && !online(epoch)) err = -ENETDOWN;
            if (!err) {
                tcp = connect_tcp(address, epoch);
                err = tcp < 0 ? tcp : 0;
                if (!err) {
                    struct websocket_request request = {.host = authority, .url = CONFIG_NEXUS_PATH,
                        .tmp_buf = http_buffer, .tmp_buf_len = sizeof(http_buffer)};
                    ws = websocket_connect(tcp, &request, 3000, NULL);
                    err = ws < 0 ? ws : 0;
                }
                if (!err && online(epoch)) {
                    source(HUD_CONNECTED);
                    backoff = 1;
                    err = receive_events(ws, epoch);
                }
            }
        }
        if (address) zsock_freeaddrinfo(address);
        source(HUD_DISCONNECTED);
        /* Wake/abort TCP before closing the websocket wrapper. No callback or
         * second thread closes a descriptor that the receiver might reuse. */
        if (tcp >= 0) zsock_shutdown(tcp, ZSOCK_SHUT_RDWR);
        if (ws >= 0) websocket_disconnect(ws);
        if (tcp >= 0) zsock_close(tcp);
        LOG_WRN("NEIA offline: %d; retry in %u s", err, backoff);
        retry_at = k_uptime_get() + backoff * 1000;
        backoff = backoff >= 8 ? 15 : backoff * 2;
    }
}
int nexus_client_start(void)
{
    if (CONFIG_NEXUS_PATH[0] != '/' || sizeof(CONFIG_NEXUS_PATH) > 256) return -EINVAL;
    if (!atomic_cas(&started, 0, 1)) return -EALREADY;
    k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), run,
                    NULL, NULL, NULL, 8, 0, K_NO_WAIT);
    k_thread_name_set(&thread, "neia_receive");
    return 0;
}
