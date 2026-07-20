#ifndef HUD_STATE_MACHINE_H
#define HUD_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>


// simplified states of the system
enum hud_state {
    HUD_STATE_BOOTING,
    HUD_STATE_CONNECTING,
    HUD_STATE_CONNECTED,
    HUD_STATE_CHECKING_SERVER_READY,
    HUD_STATE_WAITING_FOR_SYSTEM_INITIALIZED,
    HUD_STATE_READY,
    HUD_STATE_START_REQUESTED,
    HUD_STATE_WARMING_UP,
    HUD_STATE_STREAMING,
    HUD_STATE_STOP_REQUESTED,
    HUD_STATE_DRAINING,
    HUD_STATE_ERROR,
};

// recieved events
enum hud_event_type {
    HUD_EVENT_CONNECTION_STARTED,
    HUD_EVENT_SERVER_CONNECTED,
    HUD_EVENT_SERVER_DISCONNECTED,

    HUD_EVENT_SERVER_READY,
    HUD_EVENT_SYSTEM_INITIALIZED,

    HUD_EVENT_STREAM_STARTED,
    HUD_EVENT_STREAM_WARMUP_STARTED,
    HUD_EVENT_STREAM_OFFICIAL_STARTED,
    HUD_EVENT_STREAM_STARTUP_RETRY,
    HUD_EVENT_STREAM_STARTUP_FAILED,

    HUD_EVENT_COMPUTE_RESULT,

    HUD_EVENT_STREAM_STOPPED,
    HUD_EVENT_STREAM_DRAINED,

    HUD_EVENT_ERROR,
};

// a subject
struct hud_subject {
    char id[32];
    char name[32];
};

// a compute result -> latest received value for one metric
struct hud_compute_result {
    char metric_name[32];
    char unit[16];
    float latest_value;
    bool abnormal;
    bool has_value; // to distinguish between a real result and waiting to recieve

};

#define HUD_MAX_SUBJECTS 1
#define HUD_MAX_METRICS 8

struct hud_session {
    char name[48];
    char algorithm[48];
    char sensor_name[48];
    size_t sensor_count;

    struct hud_subject subjects[HUD_MAX_SUBJECTS];
    size_t subject_count;

    struct hud_compute_result results[HUD_MAX_METRICS];
    size_t result_count;
    size_t selected_result_index;
};

struct hud_event {
    enum hud_event_type type;

    const struct hud_session *session;

    const char *subject_id;
    const char *metric_name;
    const char *unit;

    float value;
    bool abnormal;

    const char *message;
};

typedef int (*hud_send_command_fn)(
    const char *command
);

int hud_state_machine_init(
    hud_send_command_fn send_command
);

int hud_state_machine_handle_event(
    const struct hud_event *event
);

void hud_state_machine_handle_single_press(void);
void hud_state_machine_handle_double_press(void);

enum hud_state hud_state_machine_get_state(void);

#endif