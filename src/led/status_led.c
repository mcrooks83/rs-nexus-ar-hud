#include "status_led.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(status_led, LOG_LEVEL_INF);

static struct k_work_delayable pattern_work;
static struct k_work_delayable identify_done_work;

static enum status_led_mode current_mode = STATUS_LED_MODE_DISCOVERY;
static enum status_led_mode saved_mode = STATUS_LED_MODE_DISCOVERY;

static bool identify_active;

static const char *mode_name(enum status_led_mode mode)
{
    switch (mode) {
    case STATUS_LED_MODE_DISCOVERY:
        return "discovery";
    case STATUS_LED_MODE_CONNECTED:
        return "connected";
    case STATUS_LED_MODE_STREAMING:
        return "streaming";
    case STATUS_LED_MODE_IDENTIFY:
        return "identify";
    case STATUS_LED_MODE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static uint32_t mode_period_ms(enum status_led_mode mode)
{
    switch (mode) {
    case STATUS_LED_MODE_DISCOVERY:
        return 1000;

    case STATUS_LED_MODE_CONNECTED:
        return 2000;

    case STATUS_LED_MODE_STREAMING:
        return 500;

    case STATUS_LED_MODE_IDENTIFY:
        return 200;

    case STATUS_LED_MODE_ERROR:
        return 300;

    default:
        return 1000;
    }
}

static void pattern_work_handler(struct k_work *work)
{
    uint32_t period_ms;

    ARG_UNUSED(work);

    /*
     * Placeholder only.
     *
     * Later this will toggle actual GPIO/PWM LEDs:
     *
     *   DISCOVERY:
     *     orange flash if RGB is available, otherwise green flash
     *
     *   CONNECTED:
     *     slow green flash
     *
     *   STREAMING:
     *     faster green flash
     *
     *   IDENTIFY:
     *     attention flash
     *
     *   ERROR:
     *     red flash if available
     */

    LOG_DBG("LED pattern tick: %s", mode_name(current_mode));

    period_ms = mode_period_ms(current_mode);
    k_work_reschedule(&pattern_work, K_MSEC(period_ms));
}

static void identify_done_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    identify_active = false;
    current_mode = saved_mode;

    LOG_INF("Identify complete, restoring LED mode: %s", mode_name(current_mode));

    k_work_reschedule(&pattern_work, K_NO_WAIT);
}

int status_led_init(void)
{
    k_work_init_delayable(&pattern_work, pattern_work_handler);
    k_work_init_delayable(&identify_done_work, identify_done_work_handler);

    current_mode = STATUS_LED_MODE_DISCOVERY;
    saved_mode = STATUS_LED_MODE_DISCOVERY;
    identify_active = false;

    LOG_INF("Status LED placeholder initialized");

    k_work_reschedule(&pattern_work, K_NO_WAIT);

    return 0;
}

void status_led_set_mode(enum status_led_mode mode)
{
    if (identify_active && mode != STATUS_LED_MODE_IDENTIFY) {
        saved_mode = mode;
        LOG_INF("Saved LED mode during identify: %s", mode_name(saved_mode));
        return;
    }

    if (current_mode == mode) {
        return;
    }

    current_mode = mode;

    LOG_INF("LED mode: %s", mode_name(current_mode));

    k_work_reschedule(&pattern_work, K_NO_WAIT);
}

void status_led_identify(uint16_t duration_ms)
{
    if (duration_ms == 0) {
        duration_ms = 1000;
    }

    if (!identify_active) {
        saved_mode = current_mode;
    }

    identify_active = true;
    current_mode = STATUS_LED_MODE_IDENTIFY;

    LOG_INF("Identify LED for %u ms", duration_ms);

    k_work_reschedule(&pattern_work, K_NO_WAIT);
    k_work_reschedule(&identify_done_work, K_MSEC(duration_ms));
}