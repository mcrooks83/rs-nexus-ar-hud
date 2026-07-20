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

#if !DT_NODE_HAS_STATUS(DISPLAY_NODE, okay)
#error "No enabled zephyr,display device found"
#endif

#define MARQUEE_STACK_SIZE  2048
#define MARQUEE_PRIORITY    7
#define MARQUEE_TEXT_MAX    64
#define MARQUEE_STEP_PIXELS 4
#define MARQUEE_DELAY_MS    1

static const struct device *display =
    DEVICE_DT_GET(DISPLAY_NODE);

static char marquee_text[MARQUEE_TEXT_MAX];

static uint16_t display_width;

/*
 * Retained in case vertical centering is restored later.
 */
static uint16_t display_height;

static uint8_t font_width;

/*
 * Retained in case vertical centering is restored later.
 */
static uint8_t font_height;

static struct k_thread marquee_thread;

static K_THREAD_STACK_DEFINE(
    marquee_stack,
    MARQUEE_STACK_SIZE
);

static bool marquee_started;

static void marquee_thread_entry(
    void *arg1,
    void *arg2,
    void *arg3
)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (true) {
        size_t text_length = strlen(marquee_text);

        int text_width =
            (int)(text_length * font_width);

        /*
         * Display the text at the top of the screen.
         */
        int y = 0;

        /*
         * To restore vertical centering, replace the line above with:
         *
         * int y =
         *     ((int)display_height - (int)font_height) / 2;
         */

        /*
         * Begin just beyond the right edge and move left.
         */
        for (int x = (int)display_width;
             x > -text_width;
             x -= MARQUEE_STEP_PIXELS) {

            int err = cfb_framebuffer_clear(
                display,
                false
            );

            if (err != 0) {
                LOG_ERR(
                    "Failed to clear framebuffer: %d",
                    err
                );

                break;
            }

            err = cfb_draw_text(
                display,
                marquee_text,
                x,
                y
            );

            if (err != 0) {
                LOG_ERR(
                    "Failed to draw marquee text: %d",
                    err
                );

                break;
            }

            err = cfb_framebuffer_finalize(display);

            if (err != 0) {
                LOG_ERR(
                    "Failed to update display: %d",
                    err
                );

                break;
            }

            k_sleep(K_MSEC(MARQUEE_DELAY_MS));
        }

        /*
         * Pause briefly before restarting at the right.
         */
        k_sleep(K_MSEC(50));
    }
}

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

    display_width = cfb_get_display_parameter(
        display,
        CFB_DISPLAY_WIDTH
    );

    /*
     * Retained for optional vertical centering.
     */
    display_height = cfb_get_display_parameter(
        display,
        CFB_DISPLAY_HEIGHT
    );

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

int display_manager_show_text(const char *text)
{
    if (text == NULL) {
        return -EINVAL;
    }

    int err = cfb_framebuffer_clear(
        display,
        false
    );

    if (err != 0) {
        return err;
    }

    err = cfb_draw_text(
        display,
        text,
        0,
        0
    );

    if (err != 0) {
        return err;
    }

    return cfb_framebuffer_finalize(display);
}

int display_manager_start_marquee(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return -EINVAL;
    }

    if (marquee_started) {
        return -EALREADY;
    }

    size_t text_length = strlen(text);

    if (text_length >= sizeof(marquee_text)) {
        return -ENAMETOOLONG;
    }

    strcpy(marquee_text, text);

    k_thread_create(
        &marquee_thread,
        marquee_stack,
        K_THREAD_STACK_SIZEOF(marquee_stack),
        marquee_thread_entry,
        NULL,
        NULL,
        NULL,
        MARQUEE_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_name_set(
        &marquee_thread,
        "oled_marquee"
    );

    marquee_started = true;

    return 0;
}

int display_manager_show_lines(
    const char *line1,
    const char *line2,
    const char *line3,
    const char *line4
)
{
    const char *lines[] = {
        line1,
        line2,
        line3,
        line4,
    };

    int err = cfb_framebuffer_clear(
        display,
        false
    );

    if (err != 0) {
        LOG_ERR(
            "Failed to clear framebuffer: %d",
            err
        );

        return err;
    }

    for (size_t index = 0;
         index < ARRAY_SIZE(lines);
         ++index) {

        if (lines[index] == NULL ||
            lines[index][0] == '\0') {
            continue;
        }

        int y =
            (int)index * (int)font_height;

        /*
         * Stop if the next line would begin outside the
         * physical display.
         */
        if (y >= (int)display_height) {
            break;
        }

        err = cfb_draw_text(
            display,
            lines[index],
            0,
            y
        );

        if (err != 0) {
            LOG_ERR(
                "Failed to draw line %u: %d",
                (unsigned int)index,
                err
            );

            return err;
        }
    }

    err = cfb_framebuffer_finalize(display);

    if (err != 0) {
        LOG_ERR(
            "Failed to update display: %d",
            err
        );
    }

    return err;
}