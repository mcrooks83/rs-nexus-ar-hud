#include "imu_measurement_service.h"

#include <errno.h>
#include <stdbool.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "ble_services.h"
#include "uuids.h"

LOG_MODULE_REGISTER(imu_measurement_svc, LOG_LEVEL_INF);

static void measurement_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    bool enabled;

    ARG_UNUSED(attr);

    enabled = (value == BT_GATT_CCC_NOTIFY);

    ble_services_set_notifications_enabled(enabled);

    LOG_INF("IMU notifications %s", enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(imu_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_IMU_SERVICE),

    BT_GATT_CHARACTERISTIC(
        BT_UUID_IMU_MEASUREMENT,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL,
        NULL,
        NULL
    ),

    BT_GATT_CCC(
        measurement_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    )
);

int imu_measurement_notify(const uint8_t packet[IMU_PACKET_SIZE_BYTES])
{
    if (!packet) {
        return -EINVAL;
    }

    if (!ble_services_notifications_enabled()) {
        return -ENOTCONN;
    }

    return bt_gatt_notify(
        ble_services_get_conn(),
        &imu_svc.attrs[1],
        packet,
        IMU_PACKET_SIZE_BYTES
    );
}