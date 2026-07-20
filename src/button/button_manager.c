#include "button_manager.h"

#include <errno.h>

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
static button_manager_callback_t app_callback;

static void button_interrupt_handler(
    const struct device *device,
    struct gpio_callback *callback,
    uint32_t pins
)
{
    ARG_UNUSED(device);
    ARG_UNUSED(callback);
    ARG_UNUSED(pins);

    LOG_INF("Button pressed");

    if (app_callback != NULL) {
        app_callback();
    }
}

int button_manager_init(button_manager_callback_t callback)
{
    if (!gpio_is_ready_dt(&button)) {
        LOG_ERR("Button GPIO is not ready");
        return -ENODEV;
    }

    app_callback = callback;

    int err = gpio_pin_configure_dt(
        &button,
        GPIO_INPUT
    );

    if (err != 0) {
        LOG_ERR("Failed to configure button: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(
        &button,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (err != 0) {
        LOG_ERR("Failed to configure button interrupt: %d", err);
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

    LOG_INF("Button initialized");

    return 0;
}