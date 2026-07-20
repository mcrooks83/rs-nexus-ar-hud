#ifndef DUMMY_NEXUS_CLIENT_H
#define DUMMY_NEXUS_CLIENT_H

#include "hud_state_machine.h"

typedef void (*dummy_nexus_event_callback_t)(
    const struct hud_event *event
);

int dummy_nexus_client_start(
    dummy_nexus_event_callback_t event_callback
);

int dummy_nexus_client_send_command(
    const char *command
);

#endif