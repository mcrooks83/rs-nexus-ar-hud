#ifndef WIFI_TEST_MGMT_H
#define WIFI_TEST_MGMT_H
#include <stdint.h>
#define WIFI_STATUS_CONN_SUCCESS 0
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PSK_MIN_LEN 8
#define WIFI_PSK_MAX_LEN 64
#define WIFI_SECURITY_TYPE_NONE 0
#define WIFI_SECURITY_TYPE_PSK 1
#define WIFI_FREQ_BAND_2_4_GHZ 0
#define WIFI_CHANNEL_ANY 0
#define WIFI_MFP_DISABLE 0
#define NET_REQUEST_WIFI_IFACE_STATUS 1
#define NET_REQUEST_WIFI_CONNECT 2
#define NET_REQUEST_WIFI_DISCONNECT 3
/* Values are deliberately distinct from wifi_link and wifi_manager_state. */
enum wifi_iface_state { WIFI_STATE_INACTIVE = 10, WIFI_STATE_DISCONNECTED,
    WIFI_STATE_INTERFACE_DISABLED, WIFI_STATE_COMPLETED, WIFI_STATE_SCANNING };
struct wifi_status { int conn_status; };
struct wifi_iface_status { enum wifi_iface_state state; };
struct wifi_connect_req_params {
    const uint8_t *ssid, *psk;
    uint8_t ssid_length, psk_length;
    int security, band, channel, mfp, timeout;
};
#endif
