#include "nexus_receive.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

void nexus_receive_reset(struct nexus_receive *r) { memset(r, 0, sizeof(*r)); }
int nexus_receive_feed(struct nexus_receive *r, const void *data, size_t size,
                       uint32_t type, uint64_t remaining)
{
    if (!r->frame_open) {
        r->frame_type = type;
        r->control_size = 0;
        if (type & (NEXUS_PING | NEXUS_PONG | NEXUS_CLOSE)) {
            if (!(type & NEXUS_FINAL) || remaining > 125 || size > 125 - remaining) return -EPROTO;
        } else {
            if (type & NEXUS_BINARY) return -EPROTO;
            if (type & NEXUS_TEXT) {
                if (r->active) return -EPROTO;
                r->size = 0; r->active = true;
            } else if (!r->active) return -EPROTO; /* orphan continuation */
        }
        r->frame_open = true;
    } else if (type != r->frame_type) return -EPROTO;
    if (type & (NEXUS_PING | NEXUS_PONG | NEXUS_CLOSE)) {
        if (size > sizeof(r->control) - r->control_size ||
            remaining > sizeof(r->control) - r->control_size - size) return -EMSGSIZE;
        memcpy(r->control + r->control_size, data, size);
        r->control_size += size;
    } else {
        if (size > NEXUS_MESSAGE_MAX - r->size || remaining > NEXUS_MESSAGE_MAX - r->size - size)
            return -EMSGSIZE;
        memcpy(r->data + r->size, data, size);
        r->size += size;
    }
    if (remaining) return NEXUS_RX_MORE;
    r->frame_open = false;
    if (type & NEXUS_CLOSE) return NEXUS_RX_CLOSE;
    if (type & NEXUS_PING) return NEXUS_RX_PING;
    if (type & NEXUS_PONG) return NEXUS_RX_PONG;
    if (type & NEXUS_FINAL) {
        r->data[r->size] = 0; r->active = false;
        return NEXUS_RX_MESSAGE;
    }
    return NEXUS_RX_MORE;
}
int nexus_receive_read(struct nexus_receive *r, const void *data, size_t size,
                       uint32_t type, uint64_t reported, uint64_t *remaining)
{
    if (type == UINT32_MAX) {
        if (!*remaining || !size || size > *remaining) return -EPROTO;
        type = r->frame_type;
        *remaining -= size;
    } else {
        if (reported == UINT64_MAX) return -EPROTO;
        *remaining = reported;
    }
    return nexus_receive_feed(r, data, size, type, *remaining);
}
int nexus_destination(bool ready, const char *gateway, const char *override,
                      char *host, size_t size)
{
    if (!host || !size || !override || !gateway) return -EINVAL;
    host[0] = 0;
    if (!ready) return -ENETDOWN;
    const char *selected = override;
    if (!*selected) {
        unsigned a, b, c, d; char trailing;
        if (sscanf(gateway, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trailing) != 4 ||
            !a || a == 127 || a >= 224 || b > 255 || c > 255 || d > 255) return -EAGAIN;
        selected = gateway;
    }
    size_t n = strlen(selected);
    if (!n || n >= size || n >= NEXUS_HOST_MAX) return -ENAMETOOLONG;
    for (size_t i = 0; i < n; ++i) {
        char c = selected[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return -EINVAL;
    }
    memcpy(host, selected, n + 1);
    return 0;
}
