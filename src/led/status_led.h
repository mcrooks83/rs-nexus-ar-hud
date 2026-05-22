#pragma once

#include <stdint.h>

enum status_led_mode {
    STATUS_LED_MODE_DISCOVERY = 0,
    STATUS_LED_MODE_CONNECTED,
    STATUS_LED_MODE_STREAMING,
    STATUS_LED_MODE_IDENTIFY,
    STATUS_LED_MODE_ERROR,
};

int status_led_init(void);

void status_led_set_mode(enum status_led_mode mode);

/*
 * Temporarily override the normal mode and flash identify color.
 * When done, the LED module returns to the previous mode.
 */
void status_led_identify(uint16_t duration_ms);