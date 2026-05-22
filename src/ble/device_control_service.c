#include "device_control_service.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "control_protocol.h"
#include "status_led.h"
#include "streamer.h"
#include "uuids.h"

LOG_MODULE_REGISTER(device_control_svc, LOG_LEVEL_INF);

static uint8_t status_buf[DEVICE_STATUS_V1_SIZE_BYTES];

static void put_u16_le(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xff);
    buf[1] = (uint8_t)((value >> 8) & 0xff);
}

static void put_u32_le(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xff);
    buf[1] = (uint8_t)((value >> 8) & 0xff);
    buf[2] = (uint8_t)((value >> 16) & 0xff);
    buf[3] = (uint8_t)((value >> 24) & 0xff);
}

static void build_status(void)
{
    status_buf[0] = streamer_is_running() ? 1 : 0;

    put_u16_le(&status_buf[1], streamer_get_odr_hz());
    put_u32_le(&status_buf[3], streamer_get_packets_sent());
    put_u32_le(&status_buf[7], streamer_get_packets_dropped());
}

static ssize_t read_status(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset)
{
    ARG_UNUSED(attr);

    build_status();

    return bt_gatt_attr_read(
        conn,
        attr,
        buf,
        len,
        offset,
        status_buf,
        sizeof(status_buf)
    );
}

static ssize_t write_command(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    const void *buf,
    uint16_t len,
    uint16_t offset,
    uint8_t flags)
{
    const uint8_t *bytes = buf;
    uint8_t opcode;

    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    opcode = bytes[CONTROL_OPCODE_OFFSET];

    switch (opcode) {
    case CTRL_START:
        if (len != CONTROL_START_SIZE_BYTES) {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        }

        streamer_start();
        LOG_INF("Command: START");
        break;

    case CTRL_STOP:
        if (len != CONTROL_STOP_SIZE_BYTES) {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        }

        streamer_stop();
        LOG_INF("Command: STOP");
        break;

    case CTRL_SET_ODR: {
        uint16_t odr_hz;

        if (len != CONTROL_SET_ODR_SIZE_BYTES) {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        }

        odr_hz = control_get_le16(bytes);

        if (streamer_set_odr_hz(odr_hz) != 0) {
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
        }

        LOG_INF("Command: SET_ODR %u", odr_hz);
        break;
    }

    case CTRL_IDENTIFY: {
        uint16_t duration_ms = 5000;

        if (len == CONTROL_IDENTIFY_SIZE_BYTES) {
            duration_ms = control_get_le16(bytes);
        } else if (len != 1) {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
        }

        status_led_identify(duration_ms);

        LOG_INF("Command: IDENTIFY %u ms", duration_ms);
        break;
    }

    case CTRL_GET_STATUS:
        /*
         * Status is exposed as a readable characteristic.
         * This opcode is accepted as a no-op for clients that prefer command-style flows.
         */
        LOG_INF("Command: GET_STATUS");
        break;

    default:
        LOG_WRN("Unknown command opcode: 0x%02x", opcode);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    return len;
}

BT_GATT_SERVICE_DEFINE(device_control_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DEVICE_CONTROL_SERVICE),

    BT_GATT_CHARACTERISTIC(
        BT_UUID_DEVICE_CONTROL_COMMAND,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL,
        write_command,
        NULL
    ),

    BT_GATT_CHARACTERISTIC(
        BT_UUID_DEVICE_STATUS,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_status,
        NULL,
        NULL
    )
);

int device_control_service_init(void)
{
    return 0;
}