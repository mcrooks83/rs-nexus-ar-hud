#pragma once

#include <stdbool.h>

struct bt_conn;

int ble_services_init(void);

struct bt_conn *ble_services_get_conn(void);

bool ble_services_notifications_enabled(void);
void ble_services_set_notifications_enabled(bool enabled);