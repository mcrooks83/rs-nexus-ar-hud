#include "button_manager.h"

#include <errno.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_manager, LOG_LEVEL_INF);

#define BUTTON_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
#error "Devicetree alias sw0 is not defined or enabled"
#endif

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct gpio_callback button_callback;

// the button manager callback type is defined as a pointer to a function.
static button_manager_callback_t single_callback, double_callback;
static int64_t first_press;
static bool waiting, pressed;

static void single_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    if (waiting) {
        waiting = false;
        single_callback();
    }
}

K_WORK_DELAYABLE_DEFINE(single_work, single_work_handler);

static void debounce_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    int active = gpio_pin_get_dt(&button);
    if (active < 0) return;
    if (!active) { pressed = false; return; }
    if (pressed) return;
    pressed = true;
    int64_t now = k_uptime_get();
    if (waiting && now - first_press <= 350) {
        waiting = false;
        k_work_cancel_delayable(&single_work);
        double_callback();
    } else {
        waiting = true;
        first_press = now;
        k_work_reschedule(&single_work, K_MSEC(350));
    }
}
K_WORK_DELAYABLE_DEFINE(debounce_work, debounce_handler);

//callback to handle the button interrupt, which will reschedule the debounce work to run after 30ms
static void button_interrupt_handler(const struct device *device,
                                     struct gpio_callback *callback, uint32_t pins)
{
    ARG_UNUSED(device); ARG_UNUSED(callback); ARG_UNUSED(pins);
    k_work_reschedule(&debounce_work, K_MSEC(30));
}

int button_manager_init(button_manager_callback_t single, button_manager_callback_t double_press)
{
    if (!gpio_is_ready_dt(&button)) {
        LOG_ERR("Button GPIO is not ready");
        return -ENODEV;
    }

    if (!single || !double_press) return -EINVAL;

    single_callback = single;
    double_callback = double_press;

    int err = gpio_pin_configure_dt(
        &button,
        GPIO_INPUT
    );

    if (err != 0) {
        LOG_ERR("Failed to configure button: %d", err);
        return err;
    }

    gpio_init_callback(
        &button_callback,
        button_interrupt_handler,
        BIT(button.pin)
    );

    err = gpio_add_callback(
        button.port,
        &button_callback
    );

    if (err != 0) {
        LOG_ERR("Failed to register button callback: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(
        &button,
        GPIO_INT_EDGE_BOTH
    );

    if (err != 0) {
        LOG_ERR("Failed to configure button interrupt: %d", err);
        return err;
    }

    LOG_INF("Button initialized");

    return 0;
}