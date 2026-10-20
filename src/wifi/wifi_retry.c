#include "wifi_retry.h"

void wifi_retry_init(struct wifi_retry *r, int64_t now)
{
    *r = (struct wifi_retry) {.phase = WIFI_RETRY_WAIT, .deadline = now,
                              .next_delay_ms = 1000};
}
static enum wifi_retry_action cancel(struct wifi_retry *r, int64_t now)
{
    r->phase = WIFI_RETRY_CANCEL;
    r->deadline = now + WIFI_CANCEL_INTERVAL_MS;
    return WIFI_RETRY_DISCONNECT;
}
enum wifi_retry_action wifi_retry_step(struct wifi_retry *r, int64_t now,
                                      enum wifi_link link, bool failed_or_lost)
{
    switch (r->phase) {
    case WIFI_RETRY_ATTEMPT:
        if (failed_or_lost) return cancel(r, now);
        if (link == WIFI_LINK_IP) {
            r->phase = WIFI_RETRY_READY;
            r->next_delay_ms = 1000;
        } else if (now >= r->deadline) return cancel(r, now);
        break;
    case WIFI_RETRY_READY:
        if (failed_or_lost || link != WIFI_LINK_IP) return cancel(r, now);
        break;
    case WIFI_RETRY_CANCEL:
        /* No new connection until the old driver attempt has actually ended.
         * Late IP/association events during cancellation cannot restore READY. */
        if (link == WIFI_LINK_IDLE) {
            r->phase = WIFI_RETRY_WAIT;
            r->deadline = now + r->next_delay_ms;
            r->next_delay_ms = r->next_delay_ms >= 8000 ? 15000 : r->next_delay_ms * 2;
        } else if (now >= r->deadline) return cancel(r, now);
        break;
    case WIFI_RETRY_WAIT:
        /* Duplicate failure/disconnect notifications must not move the timer. */
        if (now < r->deadline) break;
        if (link != WIFI_LINK_IDLE) return cancel(r, now);
        r->phase = WIFI_RETRY_ATTEMPT;
        r->deadline = now + WIFI_ATTEMPT_TIMEOUT_MS;
        return WIFI_RETRY_CONNECT;
    }
    return WIFI_RETRY_NONE;
}
