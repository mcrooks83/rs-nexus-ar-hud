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
static bool bounded(const char *s, size_t size)
{
    return s[0] && memchr(s, 0, size);
}
void hud_model_init(struct hud_model *m, bool dummy)
{
    memset(m, 0, sizeof(*m));
    m->state = HUD_STATE_BOOTING;
    m->dummy = dummy;
}
static int slot(const struct hud_model *m, const char *address)
{
    for (unsigned i = 0; i < m->count; ++i)
        if (!strcmp(m->results[i].address, address)) return (int)i;
    return -1;
}
int hud_model_apply(struct hud_model *m, const struct hud_event *e, int64_t now)
{
    m->outgoing = HUD_CMD_NONE; /* Command infrastructure retained, disabled in this phase. */
    m->pulse = false;
    if (e->type != HUD_EVENT_SYSTEM_INITIALIZED && e->type != HUD_EVENT_SENSOR_CONNECTED &&
        e->type != HUD_EVENT_COMPUTE_RESULT && e->subject_id[0]) {
        if (!bounded(e->subject_id, sizeof(e->subject_id))) return -EINVAL;
        if (!m->known_session || strcmp(e->subject_id, m->subject_id)) return -ENOENT;
    }
    switch (e->type) {
    case HUD_EVENT_WIFI:
    case HUD_EVENT_SOURCE:
        if (e->connection < HUD_DISCONNECTED || e->connection > HUD_CONNECTED) return -EINVAL;
        if (e->type == HUD_EVENT_WIFI) m->wifi = e->connection;
        else {
            m->source = e->connection;
            m->dummy = e->dummy;
            if (m->state == HUD_STATE_BOOTING) m->state = HUD_STATE_WAITING;
        }
        if ((e->type == HUD_EVENT_SOURCE || !m->dummy) && e->connection != HUD_CONNECTED)
            for (unsigned i = 0; i < m->count; ++i) m->results[i].stale = true;
        break;
    case HUD_EVENT_SERVER_READY:
        if (!m->known_session) m->state = HUD_STATE_WAITING;
        break;
    case HUD_EVENT_SYSTEM_INITIALIZED:
        if (!bounded(e->subject_id, sizeof(e->subject_id))) return -EINVAL;
        text(m->session_name, sizeof(m->session_name), e->session_name, sizeof(e->session_name));
        strcpy(m->subject_id, e->subject_id);
        memset(m->results, 0, sizeof(m->results));
        m->count = m->selected = 0;
        m->inspect_sensor = false;
        m->known_session = true;
        m->pending = HUD_CMD_NONE;
        m->state = HUD_STATE_READY;
        break;
    case HUD_EVENT_SENSOR_CONNECTED: {
        if (!bounded(e->subject_id, sizeof(e->subject_id))) return -EINVAL;
        if (!m->known_session || strcmp(e->subject_id, m->subject_id)) return -ENOENT;
        if (!e->sensor_count || e->sensor_count > HUD_MAX_SENSORS) return -ENOSPC;
        unsigned added = 0;
        for (unsigned i = 0; i < e->sensor_count; ++i) {
            const struct hud_sensor *s = &e->sensors[i];
            if (!bounded(s->address, sizeof(s->address)) ||
                !bounded(s->location, sizeof(s->location))) return -EINVAL;
            bool duplicate = false;
            for (unsigned j = 0; j < i; ++j)
                if (!strcmp(s->address, e->sensors[j].address)) duplicate = true;
            if (!duplicate && slot(m, s->address) < 0) ++added;
        }
        if (m->count + added > HUD_MAX_SENSORS) return -ENOSPC;
        /* Validate the whole batch before merging; repeat events keep values. */
        for (unsigned i = 0; i < e->sensor_count; ++i) {
            int index = slot(m, e->sensors[i].address);
            if (index < 0) index = m->count++;
            strcpy(m->results[index].address, e->sensors[i].address);
            strcpy(m->results[index].location, e->sensors[i].location);
        }
        break;
    }
    case HUD_EVENT_STREAM_STARTED:
    case HUD_EVENT_STREAM_WARMUP_STARTED:
    case HUD_EVENT_STREAM_STARTUP_RETRY:
        if (m->state != HUD_STATE_WARMING_UP)
            for (unsigned i = 0; i < m->count; ++i) m->results[i].has_value = false;
        m->state = HUD_STATE_WARMING_UP;
        m->inspect_sensor = false;
        break;
    case HUD_EVENT_STREAM_OFFICIAL_STARTED:
        m->state = HUD_STATE_STREAMING;
        m->inspect_sensor = false;
        break;
    case HUD_EVENT_COMPUTE_RESULT: {
        if (!bounded(e->subject_id, sizeof(e->subject_id)) ||
            !bounded(e->address, sizeof(e->address)) || !isfinite(e->value)) return -EINVAL;
        if (!m->known_session || strcmp(m->subject_id, e->subject_id)) return -ENOENT;
        int index = slot(m, e->address);
        if (index < 0) return -ENOENT;
        struct hud_result *r = &m->results[index];
        r->value = e->value; r->updated_ms = now;
        r->has_value = true; r->stale = false;
        m->pulse = true;
        break;
    }
    case HUD_EVENT_STREAM_STOPPED:
        m->state = HUD_STATE_DRAINING; m->inspect_sensor = false; break;
    case HUD_EVENT_STREAM_DRAINED:
        m->state = m->known_session ? HUD_STATE_READY : HUD_STATE_WAITING;
        m->inspect_sensor = false;
        break;
    case HUD_EVENT_STREAM_STARTUP_FAILED:
    case HUD_EVENT_ERROR:
        text(m->error, sizeof(m->error), e->message, sizeof(e->message));
        if (!m->error[0]) strcpy(m->error, e->type == HUD_EVENT_ERROR ? "Source error" : "Start failed");
        m->state = HUD_STATE_ERROR; m->inspect_sensor = false;
        break;
    case HUD_EVENT_BUTTON:
    case HUD_EVENT_DOUBLE_BUTTON:
        if (m->state == HUD_STATE_ERROR || m->state == HUD_STATE_DRAINING) break;
        if (m->count) {
            m->selected = (m->selected + 1) % m->count;
            m->inspect_sensor = true;
        }
        break;
    default: return -ENOTSUP;
    }
    return 0;
}
void hud_model_screen(const struct hud_model *m, int64_t now, bool blink, struct hud_screen *s)
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
    case HUD_STATE_STREAMING: a = "LIVE"; b = "Waiting data"; break;
    }
    if (m->count && m->state != HUD_STATE_ERROR && m->state != HUD_STATE_DRAINING &&
        (m->state == HUD_STATE_READY || m->state == HUD_STATE_STREAMING || m->inspect_sensor)) {
        const struct hud_result *r = &m->results[m->selected];


        a = !strcmp(r->location, "LEFT_ANKLE") ? "Left ankle" :
            !strcmp(r->location, "RIGHT_ANKLE") ? "Right ankle" : 
            !strcmp(r->location, "LEFT_WRIST") ? "Left wrist" :
            !strcmp(r->location, "RIGHT_WRIST") ? "Right wrist" : 
            !strcmp(r->location, "HEAD") ? "Head" : r->location;
        
        
            bool stale = r->stale || m->source != HUD_CONNECTED ||
            (!m->dummy && m->wifi != HUD_CONNECTED) || now - r->updated_ms >= HUD_STALE_MS;
        b = r->has_value && stale ? "LI 0-6 STALE" : "LI 0-6 Hz";
        c = "No events";
        if (r->has_value) {
            int length = snprintf(s->lines[2], sizeof(s->lines[2]), "%.2f", (double)r->value);
            if (length < 0 || length >= (int)sizeof(s->lines[2]))
                snprintf(s->lines[2], sizeof(s->lines[2]), "%.3g", (double)r->value);
            c = "";
        }
    }
    text(s->lines[0], sizeof(s->lines[0]), a, HUD_COLS);
    text(s->lines[1], sizeof(s->lines[1]), b, HUD_COLS);
    if (*c) text(s->lines[2], sizeof(s->lines[2]), c, HUD_COLS);
    /* The dummy is a build-time alternative, always visibly labelled. */
    if (m->dummy) {
        char label[HUD_COLS + 1];
        snprintf(label, sizeof(label), "Demo %.7s", s->lines[0]);
        strcpy(s->lines[0], label);
    }
}
