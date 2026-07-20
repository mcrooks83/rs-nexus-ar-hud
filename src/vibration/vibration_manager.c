#include "vibration_manager.h"

#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(vibration_manager, LOG_LEVEL_INF);

#define VIBRATION_NODE DT_ALIAS(vibration0)

#if !DT_NODE_HAS_STATUS(VIBRATION_NODE, okay)
#error "Devicetree alias vibration0 is not defined or enabled"
#endif

static const struct gpio_dt_spec vibration =
    GPIO_DT_SPEC_GET(VIBRATION_NODE, gpios);

static void vibration_stop_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(
    vibration_stop_work,
    vibration_stop_work_handler
);

static void vibration_stop_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    LOG_INF("Stopping vibration motor");

    int err = gpio_pin_set_dt(&vibration, 0);

    if (err != 0) {
        LOG_ERR("Failed to stop vibration motor: %d", err);
    }
}

int vibration_manager_init(void)
{
    if (!gpio_is_ready_dt(&vibration)) {
        LOG_ERR("Vibration GPIO is not ready");
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(
        &vibration,
        GPIO_OUTPUT_INACTIVE
    );

    if (err != 0) {
        LOG_ERR("Failed to configure vibration motor: %d", err);
        return err;
    }

    LOG_INF("Vibration motor initialized");

    return 0;
}

int vibration_manager_on(void)
{
    k_work_cancel_delayable(&vibration_stop_work);

    return gpio_pin_set_dt(&vibration, 1);
}

int vibration_manager_off(void)
{
    k_work_cancel_delayable(&vibration_stop_work);

    return gpio_pin_set_dt(&vibration, 0);
}

int vibration_manager_pulse(uint32_t duration_ms)
{
    if (duration_ms == 0U) {
        return -EINVAL;
    }

    k_work_cancel_delayable(&vibration_stop_work);

    LOG_INF(
        "Starting vibration motor for %u ms",
        duration_ms
    );

    int err = gpio_pin_set_dt(&vibration, 1);

    if (err != 0) {
        LOG_ERR("Failed to start vibration motor: %d", err);
        return err;
    }

    int result = k_work_reschedule(
        &vibration_stop_work,
        K_MSEC(duration_ms)
    );

    if (result < 0) {
        LOG_ERR(
            "Failed to schedule motor stop: %d",
            result
        );

        gpio_pin_set_dt(&vibration, 0);

        return result;
    }

    return 0;
}