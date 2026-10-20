#ifndef HUD_STATE_MACHINE_H
#define HUD_STATE_MACHINE_H
#include <stdbool.h>
#include <stdint.h>
#define HUD_COLS 12
#define HUD_MAX_SENSORS 2
#define HUD_STALE_MS 15000
#define HUD_ID_SIZE 64
#define HUD_LOCATION_SIZE 48
struct hud_sensor { char address[HUD_ID_SIZE], location[HUD_LOCATION_SIZE]; };

enum hud_connection { HUD_DISCONNECTED, HUD_CONNECTING, HUD_CONNECTED };
enum hud_state { HUD_STATE_BOOTING, HUD_STATE_WAITING, HUD_STATE_READY,
    HUD_STATE_START_REQUESTED, HUD_STATE_STOP_REQUESTED, HUD_STATE_WARMING_UP, HUD_STATE_STREAMING, HUD_STATE_DRAINING, HUD_STATE_ERROR };
enum hud_event_type {
    HUD_EVENT_WIFI, HUD_EVENT_SOURCE, HUD_EVENT_SERVER_READY,
    HUD_EVENT_SYSTEM_INITIALIZED, HUD_EVENT_STREAM_STARTED,
    HUD_EVENT_STREAM_WARMUP_STARTED, HUD_EVENT_STREAM_OFFICIAL_STARTED,
    HUD_EVENT_STREAM_STARTUP_RETRY, HUD_EVENT_STREAM_STARTUP_FAILED,
    HUD_EVENT_SENSOR_CONNECTED, HUD_EVENT_COMPUTE_RESULT, HUD_EVENT_STREAM_STOPPED,
    HUD_EVENT_STREAM_DRAINED, HUD_EVENT_ERROR, HUD_EVENT_BUTTON, HUD_EVENT_DOUBLE_BUTTON
};
/* All payloads are inline: safe to copy into a queue, never borrowed pointers. */
struct hud_event {
    enum hud_event_type type;
    enum hud_connection connection;
    bool dummy;
    char session_name[48];
    char location[HUD_LOCATION_SIZE];
    char subject_id[HUD_ID_SIZE], address[HUD_ID_SIZE];
    struct hud_sensor sensors[HUD_MAX_SENSORS];
    unsigned sensor_count;
    char message[48];
    float value;
};
struct hud_result {
    char location[HUD_LOCATION_SIZE], address[HUD_ID_SIZE];
    float value;
    bool has_value;
    bool stale;
    int64_t updated_ms;
};
enum hud_command { HUD_CMD_NONE, HUD_CMD_START, HUD_CMD_STOP };
typedef int (*hud_send_command_fn)(enum hud_command command);
struct hud_model {
    enum hud_state state;
    enum hud_connection wifi, source;
    bool dummy, known_session, inspect_sensor, pulse;
    char subject_id[HUD_ID_SIZE];
    enum hud_command pending, outgoing;
    char session_name[48], error[48];
    struct hud_result results[HUD_MAX_SENSORS];
    unsigned count, selected;
};
struct hud_screen {
    char lines[3][HUD_COLS + 1];
    enum hud_connection wifi, source;
    bool dummy, blink;
};
void hud_model_init(struct hud_model *model, bool dummy);
int hud_model_apply(struct hud_model *model, const struct hud_event *event, int64_t now);
void hud_model_screen(const struct hud_model *model, int64_t now, bool blink,
                      struct hud_screen *screen);
int hud_state_machine_init(bool dummy, hud_send_command_fn send);
/* Nonblocking producer API; reports queue overflow to caller and controller. */
int hud_state_machine_handle_event(const struct hud_event *event);
void hud_state_machine_handle_single_press(void);
void hud_state_machine_handle_double_press(void);
#endif
