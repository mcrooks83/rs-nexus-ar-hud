#include "hud_state_machine.h"
#include "display_manager.h"
#include "vibration_manager.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <string.h>
LOG_MODULE_REGISTER(hud_controller, LOG_LEVEL_INF);
K_MSGQ_DEFINE(hud_events, sizeof(struct hud_event), 32, 4);
static K_THREAD_STACK_DEFINE(controller_stack, 3072);
static struct k_thread controller_thread;
static atomic_t overflow;
static struct hud_model model;
static void process_event(const struct hud_event *event)
{
    int err = hud_model_apply(&model, event, k_uptime_get());
    if (err == -ENOENT) LOG_WRN("Event rejected: subject or sensor is not registered");
    else if (err == -ENOSPC) LOG_WRN("Event rejected: HUD supports one subject and two sensors");
    else if (err) LOG_WRN("Event %d rejected: %d", event->type, err);
    if (!err && model.pulse) {
        int pulse_err = vibration_manager_pulse(50);
        if (pulse_err) LOG_WRN("Vibration failed: %d", pulse_err);
    }
}
static void controller(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    struct hud_screen previous = {0}, screen;
    bool first = true;
    while (true) {
        int64_t now = k_uptime_get();
        if (atomic_set(&overflow, 0)) {
            struct hud_event fault = {.type = HUD_EVENT_ERROR, .message = "Queue full"};
            hud_model_apply(&model, &fault, now);
        }
        hud_model_screen(&model, now, (now / 500) % 2, &screen);
        if (first || memcmp(&screen, &previous, sizeof(screen))) {
            int err = display_manager_render(&screen);
            if (err) LOG_ERR("OLED update failed: %d", err);
            else { previous = screen; first = false; }
        }
        struct hud_event event;
        /* Timeout supplies blink and sample-age ticks, never sleeps a callback. */
        if (!k_msgq_get(&hud_events, &event, K_MSEC(500))) {
            process_event(&event);
        }
    }
}
int hud_state_machine_init(bool dummy, hud_send_command_fn send)
{
    ARG_UNUSED(send); /* No command transmission in this phase. */
    hud_model_init(&model, dummy);
    k_thread_create(&controller_thread, controller_stack,
                    K_THREAD_STACK_SIZEOF(controller_stack), controller,
                    NULL, NULL, NULL, 6, 0, K_NO_WAIT);
    k_thread_name_set(&controller_thread, "hud_controller");
    return 0;
}
int hud_state_machine_handle_event(const struct hud_event *event)
{
    if (!event) return -EINVAL;
    int err = k_msgq_put(&hud_events, event, K_NO_WAIT);
    if (err) atomic_set(&overflow, 1);
    return err;
}
void hud_state_machine_handle_single_press(void)
{
    const struct hud_event event = {.type = HUD_EVENT_BUTTON};
    hud_state_machine_handle_event(&event);
}

void hud_state_machine_handle_double_press(void)
{
    const struct hud_event event = {.type = HUD_EVENT_DOUBLE_BUTTON};
    hud_state_machine_handle_event(&event);
}
