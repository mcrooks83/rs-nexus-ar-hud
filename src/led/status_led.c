#include "status_led.h"

#include <stdbool.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(status_led, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define HAS_LED0 1
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#define HAS_LED0 0
#endif

#if DT_NODE_HAS_STATUS(LED1_NODE, okay)
#define HAS_LED1 1
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#else
#define HAS_LED1 0
#endif

#if DT_NODE_HAS_STATUS(LED2_NODE, okay)
#define HAS_LED2 1
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
#else
#define HAS_LED2 0
#endif

/*
 * Assumed mapping for XIAO BLE RGB LED.
 * If colors are wrong, swap these three assignments.
 */
#define RED_LED_INDEX   0
#define GREEN_LED_INDEX 1
#define BLUE_LED_INDEX  2

static struct k_work_delayable pattern_work;
static struct k_work_delayable identify_done_work;

static enum status_led_mode current_mode = STATUS_LED_MODE_DISCOVERY;
static enum status_led_mode saved_mode = STATUS_LED_MODE_DISCOVERY;

static bool identify_active;
static bool pulse_on;

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

static int led_set_index(uint8_t index, bool on)
{
    switch (index) {
    case 0:
#if HAS_LED0
        return gpio_pin_set_dt(&led0, on ? 1 : 0);
#else
        return 0;
#endif

    case 1:
#if HAS_LED1
        return gpio_pin_set_dt(&led1, on ? 1 : 0);
#else
        return 0;
#endif

    case 2:
#if HAS_LED2
        return gpio_pin_set_dt(&led2, on ? 1 : 0);
#else
        return 0;
#endif

    default:
        return 0;
    }
}

static void rgb_set(bool red, bool green, bool blue)
{
    led_set_index(RED_LED_INDEX, red);
    led_set_index(GREEN_LED_INDEX, green);
    led_set_index(BLUE_LED_INDEX, blue);
}

static void leds_off(void)
{
    rgb_set(false, false, false);
}

static void apply_mode_color(enum status_led_mode mode, bool on)
{
    if (!on) {
        leds_off();
        return;
    }

    switch (mode) {
    case STATUS_LED_MODE_DISCOVERY:
        /*
         * Orange if RGB mapping is correct: red + green.
         */
        rgb_set(true, true, false);
        break;

    case STATUS_LED_MODE_CONNECTED:
        rgb_set(false, true, false);
        break;

    case STATUS_LED_MODE_STREAMING:
        rgb_set(false, true, false);
        break;

    case STATUS_LED_MODE_IDENTIFY:
        /*
         * Attention color. Use red for now.
         */
        rgb_set(true, false, false);
        break;

    case STATUS_LED_MODE_ERROR:
        rgb_set(true, false, false);
        break;

    default:
        leds_off();
        break;
    }
}

static uint32_t mode_on_ms(enum status_led_mode mode)
{
    switch (mode) {
    case STATUS_LED_MODE_IDENTIFY:
        return 100;
    default:
        return 100;
    }
}

static uint32_t mode_off_ms(enum status_led_mode mode)
{
    switch (mode) {
    case STATUS_LED_MODE_DISCOVERY:
        return 900;

    case STATUS_LED_MODE_CONNECTED:
        return 1900;

    case STATUS_LED_MODE_STREAMING:
        return 400;

    case STATUS_LED_MODE_IDENTIFY:
        return 100;

    case STATUS_LED_MODE_ERROR:
        return 200;

    default:
        return 900;
    }
}

static void pattern_work_handler(struct k_work *work)
{
    uint32_t delay_ms;

    ARG_UNUSED(work);

    pulse_on = !pulse_on;
    apply_mode_color(current_mode, pulse_on);

    delay_ms = pulse_on ? mode_on_ms(current_mode) : mode_off_ms(current_mode);

    k_work_reschedule(&pattern_work, K_MSEC(delay_ms));
}

static void identify_done_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    identify_active = false;
    current_mode = saved_mode;
    pulse_on = false;
    leds_off();

    LOG_INF("Identify complete, restoring LED mode: %s", mode_name(current_mode));

    k_work_reschedule(&pattern_work, K_NO_WAIT);
}

static int configure_one_led(const struct gpio_dt_spec *led, const char *name)
{
    int err;

    if (!gpio_is_ready_dt(led)) {
        LOG_WRN("%s GPIO not ready", name);
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_WRN("Failed to configure %s: %d", name, err);
        return err;
    }

    return 0;
}

int status_led_init(void)
{
    int err;

    k_work_init_delayable(&pattern_work, pattern_work_handler);
    k_work_init_delayable(&identify_done_work, identify_done_work_handler);

#if HAS_LED0
    err = configure_one_led(&led0, "led0");
    if (err) {
        return err;
    }
#endif

#if HAS_LED1
    err = configure_one_led(&led1, "led1");
    if (err) {
        return err;
    }
#endif

#if HAS_LED2
    err = configure_one_led(&led2, "led2");
    if (err) {
        return err;
    }
#endif

#if !HAS_LED0 && !HAS_LED1 && !HAS_LED2
    LOG_WRN("No LED aliases found");
#endif

    current_mode = STATUS_LED_MODE_DISCOVERY;
    saved_mode = STATUS_LED_MODE_DISCOVERY;
    identify_active = false;
    pulse_on = false;

    leds_off();

    LOG_INF("Status LED initialized");

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
    pulse_on = false;
    leds_off();

    LOG_INF("LED mode: %s", mode_name(current_mode));

    k_work_reschedule(&pattern_work, K_NO_WAIT);
}

void status_led_identify(uint16_t duration_ms)
{
    if (duration_ms == 0) {
        duration_ms = 5000;
    }

    if (!identify_active) {
        saved_mode = current_mode;
    }

    identify_active = true;
    current_mode = STATUS_LED_MODE_IDENTIFY;
    pulse_on = false;
    leds_off();

    LOG_INF("Identify LED for %u ms", duration_ms);

    k_work_reschedule(&pattern_work, K_NO_WAIT);
    k_work_reschedule(&identify_done_work, K_MSEC(duration_ms));
}