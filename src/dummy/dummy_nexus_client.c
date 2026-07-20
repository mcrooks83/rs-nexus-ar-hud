#include "dummy_nexus_client.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dummy_nexus_client, LOG_LEVEL_INF);

#define DUMMY_THREAD_STACK_SIZE 2048
#define DUMMY_THREAD_PRIORITY   7

#define CMD_IS_SERVER_READY \
    "is_server_ready"

#define CMD_START_STREAM_FOR_ALL \
    "start_stream_for_all"

#define CMD_STOP_STREAM_FOR_ALL \
    "stop_stream_for_all"

static dummy_nexus_event_callback_t event_callback;

static struct k_thread dummy_thread;

static K_THREAD_STACK_DEFINE(
    dummy_thread_stack,
    DUMMY_THREAD_STACK_SIZE
);


// create a dummy session
static struct hud_session dummy_session = {
    .name = "Ankle Test",
    .algorithm = "Loading Intensity",
    .sensor_name = "Movella DOT",
    .sensor_count = 2,
    .subjects = {
        {
            .id = "subject-01",
            .name = "Subject 1",
        },
    },
    .subject_count = 1,
    .result_count = 0,
    .selected_result_index = 0,
};

// function to emit a dummy event
static void emit_event(
    const struct hud_event *event
)
{
    if (event_callback != NULL) {
        event_callback(event);
    }
}

static void dummy_thread_entry(
    void *arg1,
    void *arg2,
    void *arg3
)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct hud_event event = {
        .type = HUD_EVENT_CONNECTION_STARTED,
    };

    emit_event(&event);

    k_sleep(K_SECONDS(2));

    event.type = HUD_EVENT_SERVER_CONNECTED;
    emit_event(&event);

    /*
     * The state machine will later send is_server_ready.
     * For this first dummy, the connection sequence stops here
     * until that command is received.
     */

    while (true) {
        k_sleep(K_FOREVER);
    }
}

int dummy_nexus_client_start(
    dummy_nexus_event_callback_t callback
)
{
    if (callback == NULL) {
        return -EINVAL;
    }

    event_callback = callback;

    k_thread_create(
        &dummy_thread,
        dummy_thread_stack,
        K_THREAD_STACK_SIZEOF(dummy_thread_stack),
        dummy_thread_entry,
        NULL,
        NULL,
        NULL,
        DUMMY_THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_name_set(
        &dummy_thread,
        "dummy_nexus"
    );

    LOG_INF("Dummy Nexus client started");

    return 0;
}

int dummy_nexus_client_send_command(
    const char *command
)
{
    if (command == NULL) {
        return -EINVAL;
    }

    LOG_INF("Dummy command received: %s", command);

    if (strcmp(command, CMD_IS_SERVER_READY) == 0) {
        struct hud_event ready_event = {
            .type = HUD_EVENT_SERVER_READY,
        };

        emit_event(&ready_event);

        //k_sleep(K_MSEC(500));
        k_sleep(K_SECONDS(2));

        struct hud_event initialized_event = {
            .type = HUD_EVENT_SYSTEM_INITIALIZED,
            .session = &dummy_session,
        };

        emit_event(&initialized_event);

        return 0;
    }

    if (strcmp(command, CMD_START_STREAM_FOR_ALL) == 0) {
        struct hud_event event = {
            .type = HUD_EVENT_STREAM_STARTED,
        };

        emit_event(&event);

        k_sleep(K_MSEC(500));

        event.type = HUD_EVENT_STREAM_WARMUP_STARTED;
        emit_event(&event);

        k_sleep(K_SECONDS(1));

        event.type = HUD_EVENT_STREAM_OFFICIAL_STARTED;
        emit_event(&event);

        return 0;
    }

    if (strcmp(command, CMD_STOP_STREAM_FOR_ALL) == 0) {
        struct hud_event event = {
            .type = HUD_EVENT_STREAM_STOPPED,
        };

        emit_event(&event);

        k_sleep(K_MSEC(500));

        event.type = HUD_EVENT_STREAM_DRAINED;
        emit_event(&event);

        return 0;
    }

    LOG_WRN("Unsupported dummy command: %s", command);

    return -ENOTSUP;
}