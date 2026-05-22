#pragma once

#include <stdbool.h>
#include <stdint.h>

void streamer_init(void);

int streamer_set_odr_hz(uint16_t odr_hz);
uint16_t streamer_get_odr_hz(void);

void streamer_start(void);
void streamer_stop(void);
bool streamer_is_running(void);

uint32_t streamer_get_packets_sent(void);
uint32_t streamer_get_packets_dropped(void);