#include "wifi_retry.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct wifi_retry r;
    wifi_retry_init(&r, 0);
    int64_t now = 0;
    const unsigned expected[] = {1000, 2000, 4000, 8000, 15000, 15000};
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        assert(wifi_retry_step(&r, now, WIFI_LINK_IDLE, false) == WIFI_RETRY_CONNECT);
        assert(r.phase == WIFI_RETRY_ATTEMPT);
        assert(wifi_retry_step(&r, now + 1, WIFI_LINK_BUSY, false) == WIFI_RETRY_NONE);
        assert(wifi_retry_step(&r, now + 2, WIFI_LINK_BUSY, true) == WIFI_RETRY_DISCONNECT);
        // Driver remains busy after cancel: never start a second attempt.
        assert(wifi_retry_step(&r, now + 500, WIFI_LINK_BUSY, false) == WIFI_RETRY_NONE);
        now += 1000;
        assert(wifi_retry_step(&r, now, WIFI_LINK_IDLE, false) == WIFI_RETRY_NONE);
        assert(r.phase == WIFI_RETRY_WAIT && r.deadline == now + expected[i]);
        // Duplicate loss events don't postpone/accelerate retry.
        assert(wifi_retry_step(&r, now + 1, WIFI_LINK_IDLE, true) == WIFI_RETRY_NONE);
        assert(r.deadline == now + expected[i]);
        now = r.deadline;
    }
    assert(wifi_retry_step(&r, now, WIFI_LINK_IDLE, false) == WIFI_RETRY_CONNECT);
    // Association alone never resets backoff or extends the total IP deadline.
    int64_t deadline = r.deadline;
    assert(wifi_retry_step(&r, now + 30000, WIFI_LINK_ASSOCIATED, false) == WIFI_RETRY_NONE);
    assert(r.next_delay_ms == 15000 && r.deadline == deadline);
    assert(wifi_retry_step(&r, deadline, WIFI_LINK_ASSOCIATED, false) == WIFI_RETRY_DISCONNECT);
    assert(r.phase == WIFI_RETRY_CANCEL);
    assert(wifi_retry_step(&r, deadline + 1, WIFI_LINK_IP, false) == WIFI_RETRY_NONE);
    assert(r.phase == WIFI_RETRY_CANCEL); // late IP cannot cancel cleanup
    assert(wifi_retry_step(&r, deadline + 1000, WIFI_LINK_BUSY, false) == WIFI_RETRY_DISCONNECT);
    assert(wifi_retry_step(&r, deadline + 1001, WIFI_LINK_IDLE, false) == WIFI_RETRY_NONE);
    now = r.deadline;
    assert(wifi_retry_step(&r, now, WIFI_LINK_IDLE, false) == WIFI_RETRY_CONNECT);
    assert(wifi_retry_step(&r, now + 100, WIFI_LINK_IP, false) == WIFI_RETRY_NONE);
    assert(r.phase == WIFI_RETRY_READY && r.next_delay_ms == 1000);
    assert(wifi_retry_step(&r, now + 200, WIFI_LINK_IP, false) == WIFI_RETRY_NONE);
    // Loss wins over a stale IP snapshot, then recovery uses the reset backoff.
    assert(wifi_retry_step(&r, now + 300, WIFI_LINK_IP, true) == WIFI_RETRY_DISCONNECT);
    assert(wifi_retry_step(&r, now + 400, WIFI_LINK_IDLE, false) == WIFI_RETRY_NONE);
    assert(r.deadline == now + 1400);
    now = r.deadline;
    assert(wifi_retry_step(&r, now, WIFI_LINK_IDLE, false) == WIFI_RETRY_CONNECT);
    assert(wifi_retry_step(&r, now + 1, WIFI_LINK_IP, false) == WIFI_RETRY_NONE);
    assert(r.phase == WIFI_RETRY_READY);
    // Address loss without a Wi-Fi disconnect notification still recovers.
    assert(wifi_retry_step(&r, now + 2, WIFI_LINK_ASSOCIATED, false) == WIFI_RETRY_DISCONNECT);
    // Unknown/busy driver state never permits a new attempt.
    wifi_retry_init(&r, 0);
    assert(wifi_retry_step(&r, 0, WIFI_LINK_UNKNOWN, false) == WIFI_RETRY_DISCONNECT);
    for (now = 1000; now <= 100000; now += 1000)
        assert(wifi_retry_step(&r, now, WIFI_LINK_BUSY, false) == WIFI_RETRY_DISCONNECT);
    assert(r.phase == WIFI_RETRY_CANCEL);
    puts("Wi-Fi policy: backoff/cap/reset, DHCP deadline, disconnect recovery and no overlap passed");
}
