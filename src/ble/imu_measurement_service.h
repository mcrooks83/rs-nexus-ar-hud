#pragma once

#include <stdint.h>

#include "imu_packet.h"

int imu_measurement_notify(const uint8_t packet[IMU_PACKET_SIZE_BYTES]);