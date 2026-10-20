#ifndef NEXUS_CLIENT_H
#define NEXUS_CLIENT_H
#include "hud_state_machine.h"
/* One receive thread, no command transmit API. */
int nexus_client_start(void);
void nexus_client_wifi_changed(void);
#endif
