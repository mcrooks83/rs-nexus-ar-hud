#ifndef NEXUS_RECEIVE_H
#define NEXUS_RECEIVE_H
#include "nexus_event_parser.h"
/* Mirror Zephyr websocket flags; compile-time checked by the transport. */
#define NEXUS_FINAL 1
#define NEXUS_TEXT 2
#define NEXUS_BINARY 4
#define NEXUS_CLOSE 8
#define NEXUS_PING 16
#define NEXUS_PONG 32
enum nexus_rx_action { NEXUS_RX_MORE, NEXUS_RX_MESSAGE, NEXUS_RX_PING,
                       NEXUS_RX_PONG, NEXUS_RX_CLOSE };
struct nexus_receive {
    char data[NEXUS_MESSAGE_MAX + 1];
    unsigned char control[125];
    size_t size, control_size;
    bool active, frame_open;
    uint32_t frame_type;
};
void nexus_receive_reset(struct nexus_receive *rx);
int nexus_receive_feed(struct nexus_receive *rx, const void *data, size_t size,
                       uint32_t type, uint64_t remaining);
/* Normalize installed Zephyr's missing outputs on a partial-payload timeout. */
int nexus_receive_read(struct nexus_receive *rx, const void *data, size_t size,
                       uint32_t type, uint64_t reported, uint64_t *remaining);
#define NEXUS_HOST_MAX 254
int nexus_destination(bool ready, const char *gateway, const char *override,
                      char *host, size_t size);
#endif
