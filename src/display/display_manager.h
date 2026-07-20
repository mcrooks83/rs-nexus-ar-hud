#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

int display_manager_init(void);
int display_manager_show_text(const char *text);
int display_manager_start_marquee(const char *text);
int display_manager_show_lines(
    const char *line1,
    const char *line2,
    const char *line3,
    const char *line4
);

#endif