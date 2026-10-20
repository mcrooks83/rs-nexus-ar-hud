#ifndef DUMMY_NEXUS_CLIENT_H
#define DUMMY_NEXUS_CLIENT_H
#include "hud_state_machine.h"
typedef void (*dummy_nexus_event_callback_t)(const struct hud_event *event);
int dummy_nexus_client_start(dummy_nexus_event_callback_t callback);
int dummy_nexus_client_send_command(enum hud_command command);
#endif
