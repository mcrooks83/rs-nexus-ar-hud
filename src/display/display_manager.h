#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H
#include "hud_state_machine.h"

int display_manager_init(void);
/* Called exclusively by the HUD controller. Always includes the status row. */
int display_manager_render(const struct hud_screen *screen);
#endif
