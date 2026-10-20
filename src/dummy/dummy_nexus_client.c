#include "dummy_nexus_client.h"
#include <errno.h>
#include <zephyr/kernel.h>
static dummy_nexus_event_callback_t emit;
static struct k_thread thread;
static K_THREAD_STACK_DEFINE(stack, 2048);
K_MSGQ_DEFINE(commands, sizeof(enum hud_command), 8, 4);
static bool started;
static void lifecycle(enum hud_event_type type)
{
    const struct hud_event event = {.type = type};
    emit(&event);
}
static void run(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    k_sleep(K_SECONDS(2));
    struct hud_event source = {.type = HUD_EVENT_SOURCE,
        .connection = HUD_CONNECTING, .dummy = true};
    emit(&source);
    k_sleep(K_SECONDS(2));
    source.connection = HUD_CONNECTED;
    emit(&source);
    lifecycle(HUD_EVENT_SERVER_READY);
    k_sleep(K_SECONDS(3));
    const struct hud_event session = {.type = HUD_EVENT_SYSTEM_INITIALIZED,
                                      .session_name = "Ankle demo", .subject_id = "demo-subject"};
    emit(&session);
    const struct hud_event sensors = {.type = HUD_EVENT_SENSOR_CONNECTED,
        .subject_id = "demo-subject", .sensor_count = 2, .sensors = {
            {.address = "demo-left", .location = "LEFT_ANKLE"},
            {.address = "demo-right", .location = "RIGHT_ANKLE"}}};
    emit(&sensors);
    unsigned phase = 0, samples = 0;
    int64_t deadline = 0;
    while (true) {
        enum hud_command command;
        int64_t now = k_uptime_get();
        k_timeout_t timeout = phase ? K_MSEC(MAX(0, deadline - now)) : K_SECONDS(4);
        if (!k_msgq_get(&commands, &command, timeout)) {
            if (command == HUD_CMD_START && phase == 0) {
                lifecycle(HUD_EVENT_STREAM_STARTED);
                lifecycle(HUD_EVENT_STREAM_WARMUP_STARTED);
                phase = 1; deadline = k_uptime_get() + 3000;
            } else if (command == HUD_CMD_STOP && (phase == 1 || phase == 2 || phase == 3)) {
                lifecycle(HUD_EVENT_STREAM_STOPPED);
                phase = 4; deadline = k_uptime_get() + 3000;
            }
            continue;
        }
        if (phase == 0) {
            lifecycle(HUD_EVENT_STREAM_WARMUP_STARTED);
            phase = 1; samples = 0; deadline = k_uptime_get() + 3000;
        } else if (phase == 1) {
            lifecycle(HUD_EVENT_STREAM_OFFICIAL_STARTED);
            phase = 2; deadline = k_uptime_get() + 2000;
        } else if (phase == 2 || phase == 3) {
            struct hud_event left = {.type = HUD_EVENT_COMPUTE_RESULT,
                .subject_id = "demo-subject", .address = "demo-left", .location = "LEFT_ANKLE", .value = 12.48f + (samples % 5) * 0.21f};
            struct hud_event right = {.type = HUD_EVENT_COMPUTE_RESULT,
                .subject_id = "demo-subject", .address = "demo-right", .location = "RIGHT_ANKLE", .value = 11.76f + (samples % 4) * 0.18f};
            emit(&left); emit(&right); ++samples;
            phase = 3; deadline = k_uptime_get() + 1000;
            if (samples == 20) {
                lifecycle(HUD_EVENT_STREAM_STOPPED);
                phase = 4; deadline = k_uptime_get() + 3000;
            }
        } else if (phase == 4) {
            lifecycle(HUD_EVENT_STREAM_DRAINED);
            phase = 0;
        }
    }
}
int dummy_nexus_client_start(dummy_nexus_event_callback_t callback)
{
    if (!callback) return -EINVAL;
    if (started) return -EALREADY;
    started = true; emit = callback;
    k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), run,
                    NULL, NULL, NULL, 7, 0, K_NO_WAIT);
    k_thread_name_set(&thread, "dummy_nexus");
    return 0;
}
int dummy_nexus_client_send_command(enum hud_command command)
{
    if (!started) return -ENODEV;
    if (command != HUD_CMD_START && command != HUD_CMD_STOP) return -EINVAL;
    return k_msgq_put(&commands, &command, K_NO_WAIT);
}
