#include "display_manager.h"
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_manager, LOG_LEVEL_INF);
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
static const struct device *display = DEVICE_DT_GET(DISPLAY_NODE);
static uint16_t display_width, display_height;
static uint8_t font_width, font_height;
int display_manager_init(void)
{
    if (!device_is_ready(display)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    int err = display_set_pixel_format(
        display,
        PIXEL_FORMAT_MONO10
    );

    if (err != 0) {
        err = display_set_pixel_format(
            display,
            PIXEL_FORMAT_MONO01
        );

        if (err != 0) {
            LOG_ERR(
                "Failed to set display pixel format: %d",
                err
            );

            return err;
        }
    }

    err = cfb_framebuffer_init(display);

    if (err != 0) {
        LOG_ERR(
            "Failed to initialize framebuffer: %d",
            err
        );

        return err;
    }

    err = cfb_get_font_size(
        display,
        0,
        &font_width,
        &font_height
    );

    if (err != 0) {
        LOG_ERR(
            "Failed to get font size: %d",
            err
        );

        return err;
    }

    if (font_width != 10 || font_height != 16) return -ENOTSUP;
    err = cfb_framebuffer_set_font(display, 0);
    if (err) return err;

    display_width = cfb_get_display_parameter(
        display,
        CFB_DISPLAY_WIDTH
    );

    display_height = cfb_get_display_parameter(
        display,
        CFB_DISPLAY_HEIGHT
    );

    if (display_width != 128 || display_height != 64) return -ENOTSUP;

    err = cfb_framebuffer_clear(
        display,
        true
    );

    if (err != 0) {
        LOG_ERR(
            "Failed to clear display: %d",
            err
        );

        return err;
    }

    err = display_blanking_off(display);

    if (err != 0 && err != -ENOSYS) {
        LOG_ERR(
            "Failed to unblank display: %d",
            err
        );

        return err;
    }

    LOG_INF(
        "Display initialized: %ux%u, font=%ux%u",
        display_width,
        display_height,
        font_width,
        font_height
    );

    return 0;
}


static int indicator(int x, enum hud_connection state, bool blink)
{
    struct cfb_position center = {.x = x, .y = 7};
    int err = cfb_draw_circle(display, &center, 4);
    if (err) return err;
    if (state == HUD_CONNECTED || (state == HUD_CONNECTING && blink)) {
        for (int dy = -3; dy <= 3; ++dy) {
            for (int dx = -3; dx <= 3; ++dx) {
                if (dx * dx + dy * dy > 10) continue;
                struct cfb_position point = {.x = x + dx, .y = 7 + dy};
                err = cfb_draw_point(display, &point);
                if (err) return err;
            }
        }
    }
    return 0;
}
int display_manager_render(const struct hud_screen *s)
{
    int err = cfb_framebuffer_clear(display, false);
    if (err) return err;
    if ((err = cfb_draw_text(display, "Wi-Fi", 0, 0)) ||
        (err = cfb_draw_text(display, "N3", 76, 0)) ||
        (err = indicator(58, s->wifi, s->blink)) ||
        (err = indicator(108, s->source, s->blink))) return err;
    for (unsigned i = 0; i < 3; ++i) {
        if (strnlen(s->lines[i], HUD_COLS + 1) > HUD_COLS) return -EINVAL;
        if ((err = cfb_draw_text(display, s->lines[i], 0, (i + 1) * 16))) return err;
    }
    return cfb_framebuffer_finalize(display);
}
