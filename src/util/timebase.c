#include "timebase.h"

#include <zephyr/kernel.h>

uint32_t timebase_uptime_us32(void)
{
    return (uint32_t)(k_uptime_get() * 1000U);
}