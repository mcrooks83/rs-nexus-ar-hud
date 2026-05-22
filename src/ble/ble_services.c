#include "ble_services.h"

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include "app_version.h"
#include "device_control_service.h"
#include "imu_measurement_service.h"
#include "status_led.h"
#include "streamer.h"

LOG_MODULE_REGISTER(ble_services, LOG_LEVEL_INF);

static struct bt_conn *current_conn;
static bool notifications_enabled;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(
        BT_DATA_NAME_COMPLETE,
        APP_DEVICE_NAME,
        sizeof(APP_DEVICE_NAME) - 1
    ),
};

/*
 * Mirror the Arduino sketch's preferred streaming link interval.
 * At 100 Hz a slower interval can force the link to batch notifications and
 * increases notify-side backpressure.
 */
static const struct bt_le_conn_param preferred_conn_param = {
    .interval_min = 6,   /* 7.5 ms */
    .interval_max = 12,  /* 15 ms */
    .latency = 0,
    .timeout = 500,      /* 5 s */
};

static struct bt_gatt_exchange_params mtu_exchange_params;

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static const struct bt_conn_le_phy_param preferred_phy = {
    .options = BT_CONN_LE_PHY_OPT_NONE,
    .pref_tx_phy = BT_GAP_LE_PHY_2M,
    .pref_rx_phy = BT_GAP_LE_PHY_2M,
};
#endif

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static const struct bt_conn_le_data_len_param preferred_data_len = {
    .tx_max_len = CONFIG_BT_CTLR_DATA_LENGTH_MAX,
    .tx_max_time = 2120,
};
#endif

static struct k_work_delayable adv_restart_work;

static void mtu_exchanged(
    struct bt_conn *conn,
    uint8_t err,
    struct bt_gatt_exchange_params *params)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(params);

    if (err) {
        LOG_WRN("MTU exchange failed: %u", err);
        return;
    }

    LOG_INF("MTU exchange complete");
}

static int start_advertising(void)
{
    int err;

    err = bt_le_adv_start(
        BT_LE_ADV_CONN_FAST_1,
        ad,
        ARRAY_SIZE(ad),
        NULL,
        0
    );

    if (err == -EALREADY) {
        return 0;
    }

    if (err) {
        LOG_ERR("Advertising failed: %d", err);
        return err;
    }

    status_led_set_mode(STATUS_LED_MODE_DISCOVERY);

    LOG_INF("Advertising as %s", CONFIG_BT_DEVICE_NAME);

    return 0;
}

static void adv_restart_work_handler(struct k_work *work)
{
    int err;

    ARG_UNUSED(work);

    err = start_advertising();
    if (err) {
        LOG_ERR("Delayed advertising restart failed: %d", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    int update_err;

    if (err) {
        LOG_WRN("Connection failed: %u", err);
        status_led_set_mode(STATUS_LED_MODE_DISCOVERY);
        return;
    }

    current_conn = bt_conn_ref(conn);
    notifications_enabled = false;

    LOG_INF("Connected");

    status_led_set_mode(STATUS_LED_MODE_CONNECTED);

    update_err = bt_conn_le_param_update(conn, &preferred_conn_param);
    if (update_err) {
        LOG_WRN("Connection parameter update failed: %d", update_err);
    }

#if defined(CONFIG_BT_USER_PHY_UPDATE)
    update_err = bt_conn_le_phy_update(conn, &preferred_phy);
    if (update_err) {
        LOG_WRN("PHY update request failed: %d", update_err);
    }
#endif

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
    update_err = bt_conn_le_data_len_update(conn, &preferred_data_len);
    if (update_err) {
        LOG_WRN("Data length update request failed: %d", update_err);
    }
#endif

    mtu_exchange_params.func = mtu_exchanged;
    update_err = bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
    if (update_err) {
        LOG_WRN("MTU exchange request failed: %d", update_err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_INF("Disconnected: %u", reason);

    notifications_enabled = false;

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    streamer_stop();

    status_led_set_mode(STATUS_LED_MODE_DISCOVERY);

    /*
     * Restart advertising shortly after the disconnect callback returns.
     */
    k_work_reschedule(&adv_restart_work, K_MSEC(250));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

void ble_services_set_notifications_enabled(bool enabled)
{
    notifications_enabled = enabled;
}

bool ble_services_notifications_enabled(void)
{
    return current_conn && notifications_enabled;
}

struct bt_conn *ble_services_get_conn(void)
{
    return current_conn;
}

int ble_services_init(void)
{
    int err;

    k_work_init_delayable(&adv_restart_work, adv_restart_work_handler);

    /*
     * The custom GATT services are statically registered by
     * BT_GATT_SERVICE_DEFINE(), but call init hooks anyway so future
     * service-specific setup has a home.
     */
    err = device_control_service_init();
    if (err) {
        LOG_ERR("device_control_service_init failed: %d", err);
        return err;
    }

    

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("bt_enable failed: %d", err);
        return err;
    }

    LOG_INF("Bluetooth enabled");

    return start_advertising();
}
