#ifndef WIFI_RETRY_H
#define WIFI_RETRY_H
#include <stdbool.h>
#include <stdint.h>

#define WIFI_ATTEMPT_TIMEOUT_MS 45000
#define WIFI_CANCEL_INTERVAL_MS 1000

enum wifi_link { WIFI_LINK_UNKNOWN, WIFI_LINK_IDLE, WIFI_LINK_BUSY,
                 WIFI_LINK_ASSOCIATED, WIFI_LINK_IP };
enum wifi_retry_phase { WIFI_RETRY_WAIT, WIFI_RETRY_ATTEMPT,
                        WIFI_RETRY_READY, WIFI_RETRY_CANCEL };
enum wifi_retry_action { WIFI_RETRY_NONE, WIFI_RETRY_CONNECT, WIFI_RETRY_DISCONNECT };
struct wifi_retry {
    enum wifi_retry_phase phase;
    int64_t deadline;
    unsigned next_delay_ms;
};
/* Owned by the single Wi-Fi worker. No Zephyr, networking or rendering here. */
void wifi_retry_init(struct wifi_retry *retry, int64_t now);
enum wifi_retry_action wifi_retry_step(struct wifi_retry *retry, int64_t now,
                                      enum wifi_link link, bool failed_or_lost);
#endif
