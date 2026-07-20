#ifndef VIBRATION_MANAGER_H
#define VIBRATION_MANAGER_H

#include <stdint.h>

int vibration_manager_init(void);
int vibration_manager_on(void);
int vibration_manager_off(void);
int vibration_manager_pulse(uint32_t duration_ms);

#endif