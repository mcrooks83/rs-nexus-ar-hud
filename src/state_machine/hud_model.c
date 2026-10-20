#include "hud_state_machine.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void text(char *out, size_t size, const char *in, size_t limit)
{
    size_t i = 0;
    while (i + 1 < size && i < limit && in[i]) {
        unsigned char c = (unsigned char)in[i];
        out[i] = c >= 32 && c <= 126 ? c : '?';
        ++i;
    }
    out[i] = 0;
}
void hud_model_init(struct hud_model *m, bool dummy)
{
    memset(m, 0, sizeof(*m));
    m->state = HUD_STATE_BOOTING;
    m->dummy = dummy;
}
int hud_model_apply(struct hud_model *m, const struct hud_event *e, int64_t now)
{
    m->outgoing = HUD_CMD_NONE;
    switch (e->type) {
    case HUD_EVENT_WIFI:
    case HUD_EVENT_SOURCE:
        if (e->connection < HUD_DISCONNECTED || e->connection > HUD_CONNECTED)
            return -EINVAL;
        if (e->type == HUD_EVENT_WIFI) m->wifi = e->connection;
        else { m->source = e->connection; m->dummy = e->dummy; }
        /* A loss invalidates retained samples until each receives fresh data.
         * Dummy samples do not depend on Wi-Fi. Lifecycle is never inferred. */
        if ((e->type == HUD_EVENT_SOURCE || !m->dummy) &&
            e->connection != HUD_CONNECTED) {
            for (unsigned i = 0; i < m->count; ++i) m->results[i].stale = true;
        }
        break;
    case HUD_EVENT_SERVER_READY:
        if (!m->known_session) m->state = HUD_STATE_WAITING;
        break;
    case HUD_EVENT_SYSTEM_INITIALIZED:
        text(m->session_name, sizeof(m->session_name), e->session_name,
             sizeof(e->session_name));
        memset(m->results, 0, sizeof(m->results));
        m->count = m->selected = 0;
        m->known_session = true;
        m->pending = HUD_CMD_NONE;
        m->state = HUD_STATE_READY;
        break;
    case HUD_EVENT_STREAM_STARTED:
    case HUD_EVENT_STREAM_WARMUP_STARTED:
    case HUD_EVENT_STREAM_STARTUP_RETRY:
        if (m->pending == HUD_CMD_START) m->pending = HUD_CMD_NONE;
        m->state = m->pending == HUD_CMD_STOP ? HUD_STATE_STOP_REQUESTED : HUD_STATE_WARMING_UP;
        break;
    case HUD_EVENT_STREAM_OFFICIAL_STARTED:
        /* New acquisition must not present the previous run as fresh data. */
        for (unsigned i = 0; i < m->count; ++i) m->results[i].has_value = false;
        if (m->pending == HUD_CMD_START) m->pending = HUD_CMD_NONE;
        m->state = m->pending == HUD_CMD_STOP ? HUD_STATE_STOP_REQUESTED : HUD_STATE_STREAMING;
        break;
    case HUD_EVENT_COMPUTE_RESULT: {
        if (m->state != HUD_STATE_STREAMING && m->state != HUD_STATE_STOP_REQUESTED) return 0;
        if (!isfinite(e->value) || !e->location[0]) return -EINVAL;
        char location[24];
        text(location, sizeof(location), e->location, sizeof(e->location));
        unsigned i;
        for (i = 0; i < m->count; ++i)
            if (!strcmp(m->results[i].location, location)) break;
        if (i == m->count) {
            if (m->count == HUD_MAX_SENSORS) return -ENOSPC;
            ++m->count;
            strcpy(m->results[i].location, location);
        }
        m->results[i].value = e->value;
        m->results[i].updated_ms = now;
        m->results[i].has_value = true;
        m->results[i].stale = false;
        break;
    }
    case HUD_EVENT_STREAM_STOPPED:
        m->pending = HUD_CMD_NONE; m->state = HUD_STATE_DRAINING; break;
    case HUD_EVENT_STREAM_DRAINED:
        m->pending = HUD_CMD_NONE;
        m->state = m->known_session ? HUD_STATE_READY : HUD_STATE_WAITING;
        break;
    case HUD_EVENT_STREAM_STARTUP_FAILED:
    case HUD_EVENT_ERROR:
        text(m->error, sizeof(m->error), e->message, sizeof(e->message));
        if (!m->error[0]) strcpy(m->error, e->type == HUD_EVENT_ERROR ? "Source error" : "Start failed");
        m->pending = HUD_CMD_NONE;
        m->state = HUD_STATE_ERROR;
        break;
    case HUD_EVENT_BUTTON:
        if (m->source != HUD_CONNECTED || (!m->dummy && m->wifi != HUD_CONNECTED)) break;
        if (m->pending == HUD_CMD_STOP) break;
        if (m->state == HUD_STATE_READY && m->pending == HUD_CMD_NONE) {
            m->pending = m->outgoing = HUD_CMD_START;
            m->state = HUD_STATE_START_REQUESTED;
        } else if (m->state == HUD_STATE_START_REQUESTED ||
                   m->state == HUD_STATE_WARMING_UP || m->state == HUD_STATE_STREAMING) {
            m->pending = m->outgoing = HUD_CMD_STOP;
            m->state = HUD_STATE_STOP_REQUESTED;
        }
        break;
    case HUD_EVENT_DOUBLE_BUTTON:
        if (m->state == HUD_STATE_STREAMING && m->count) m->selected = (m->selected + 1) % m->count;
        break;
    default: return -ENOTSUP;
    }
    return 0;
}
void hud_model_screen(const struct hud_model *m, int64_t now, bool blink,
                      struct hud_screen *s)
{
    memset(s, 0, sizeof(*s));
    s->wifi = m->wifi; s->source = m->source; s->dummy = m->dummy;
    s->blink = blink && (m->wifi == HUD_CONNECTING || m->source == HUD_CONNECTING);
    const char *a = "", *b = "", *c = "";
    switch (m->state) {
    case HUD_STATE_BOOTING: a = "NEXUS HUD"; b = "Booting"; break;
    case HUD_STATE_WAITING: a = "Waiting for"; b = "status"; break;
    case HUD_STATE_READY: a = "Ready"; b = m->session_name; break;
    case HUD_STATE_START_REQUESTED: a = "Start sent"; b = "Waiting"; break;
    case HUD_STATE_STOP_REQUESTED: a = "Stop sent"; b = "Waiting"; break;
    case HUD_STATE_WARMING_UP: a = "Warming up"; break;
    case HUD_STATE_DRAINING: a = "Finalizing"; break;
    case HUD_STATE_ERROR: a = "ERROR"; b = m->error; break;
    case HUD_STATE_STREAMING:
        a = "LIVE"; b = "Waiting data";
        if (m->count && m->results[m->selected].has_value) {
            const struct hud_result *r = &m->results[m->selected];
            bool stale = r->stale || m->source != HUD_CONNECTED ||
                (!m->dummy && m->wifi != HUD_CONNECTED) || now - r->updated_ms >= HUD_STALE_MS;
            a = r->location;
            b = stale ? "LI 0-6 STALE" : "LI 0-6 Hz";
            /* Bound numeric output, including values from a future source. */
            if (r->value > 99999 || r->value < -9999) c = "Out of range";
            else snprintf(s->lines[2], sizeof(s->lines[2]), "%.2f", (double)r->value);
        }
        break;
    }
    text(s->lines[0], sizeof(s->lines[0]), a, HUD_COLS);
    text(s->lines[1], sizeof(s->lines[1]), b, HUD_COLS);
    if (*c) text(s->lines[2], sizeof(s->lines[2]), c, HUD_COLS);
    /*if (m->dummy) {
        if (!s->lines[2][0]) strcpy(s->lines[2], "Demo");
        else if (m->state == HUD_STATE_STREAMING && !*c) {
            char value[HUD_COLS + 1];
            strcpy(value, s->lines[2]);
            if (strlen(value) > HUD_COLS - 5) {
                snprintf(value, sizeof(value), "%.0e",
                         (double)m->results[m->selected].value);
            }
            snprintf(s->lines[2], sizeof(s->lines[2]), "Demo %.7s", value);
        } else strcpy(s->lines[2], "Demo range");
    }*/
}
