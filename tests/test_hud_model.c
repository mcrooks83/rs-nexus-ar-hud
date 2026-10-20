#include "hud_state_machine.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct hud_model m;
static void event(enum hud_event_type type)
{
    const struct hud_event e = {.type = type};
    assert(hud_model_apply(&m, &e, 100) == 0);
}
static struct hud_screen screen(int64_t now)
{
    struct hud_screen s;
    hud_model_screen(&m, now, true, &s);
    for (unsigned i = 0; i < 3; ++i) {
        assert(strlen(s.lines[i]) <= HUD_COLS);
        for (unsigned j = 0; s.lines[i][j]; ++j)
            assert(s.lines[i][j] >= 32 && s.lines[i][j] <= 126);
    }
    return s;
}
static void connection(enum hud_event_type type, enum hud_connection state, bool dummy)
{
    const struct hud_event e = {.type = type, .connection = state, .dummy = dummy};
    assert(hud_model_apply(&m, &e, 100) == 0);
}
static void ready(bool dummy)
{
    hud_model_init(&m, dummy);
    connection(HUD_EVENT_SOURCE, HUD_CONNECTED, dummy);
    connection(HUD_EVENT_WIFI, HUD_CONNECTED, dummy);
    const struct hud_event session = {.type = HUD_EVENT_SYSTEM_INITIALIZED,
        .session_name = "A lengthy ankle demo session"};
    assert(hud_model_apply(&m, &session, 0) == 0);
}
static void result(const char *location, float value)
{
    struct hud_event e = {.type = HUD_EVENT_COMPUTE_RESULT, .value = value};
    snprintf(e.location, sizeof(e.location), "%s", location);
    assert(hud_model_apply(&m, &e, 100) == 0);
}
int main(void)
{
    hud_model_init(&m, true);
    struct hud_screen s = screen(0);
    assert(!strcmp(s.lines[0], "NEXUS HUD") && s.dummy);
    assert(!s.lines[2][0]);
    event(HUD_EVENT_SERVER_READY);
    s = screen(0); assert(!strcmp(s.lines[0], "Waiting for"));
    ready(true);
    s = screen(0); assert(!strcmp(s.lines[0], "Ready"));
    assert(!strcmp(s.lines[1], "A lengthy an"));
    /* Simulated controls remain functional without an AP. */
    connection(HUD_EVENT_WIFI, HUD_DISCONNECTED, true);
    event(HUD_EVENT_BUTTON);
    assert(m.state == HUD_STATE_START_REQUESTED && m.outgoing == HUD_CMD_START);
    s = screen(100); assert(!strcmp(s.lines[0], "Start sent"));
    /* Stop is allowed before start confirmation, and is never repeated. */
    event(HUD_EVENT_BUTTON);
    assert(m.state == HUD_STATE_STOP_REQUESTED && m.outgoing == HUD_CMD_STOP);
    event(HUD_EVENT_BUTTON); assert(m.outgoing == HUD_CMD_NONE);
    event(HUD_EVENT_STREAM_WARMUP_STARTED);
    assert(m.state == HUD_STATE_STOP_REQUESTED);
    event(HUD_EVENT_STREAM_OFFICIAL_STARTED);
    assert(m.state == HUD_STATE_STOP_REQUESTED);
    event(HUD_EVENT_STREAM_STOPPED); assert(m.state == HUD_STATE_DRAINING);
    event(HUD_EVENT_BUTTON); assert(m.outgoing == HUD_CMD_NONE);
    event(HUD_EVENT_STREAM_DRAINED); assert(m.state == HUD_STATE_READY);
    event(HUD_EVENT_BUTTON); assert(m.outgoing == HUD_CMD_START);
    event(HUD_EVENT_STREAM_WARMUP_STARTED); assert(m.state == HUD_STATE_WARMING_UP);
    event(HUD_EVENT_STREAM_OFFICIAL_STARTED); assert(m.state == HUD_STATE_STREAMING);
    s = screen(100); assert(!strcmp(s.lines[1], "Waiting data"));
    result("Left ankle", 12.48f); result("Right ankle", 11.76f);
    s = screen(100); assert(!strcmp(s.lines[2], "12.48"));
    result("Left ankle", 99999);
    s = screen(100); assert(!strcmp(s.lines[2], "99999.00"));
    result("Left ankle", 1e30f);
    s = screen(100); assert(!strcmp(s.lines[2], "Out of range"));
    result("Left ankle", 12.48f);
    event(HUD_EVENT_DOUBLE_BUTTON);
    assert(m.selected == 1 && m.outgoing == HUD_CMD_NONE && m.state == HUD_STATE_STREAMING);
    s = screen(100); assert(!strcmp(s.lines[0], "Right ankle"));
    assert(!strcmp(s.lines[2], "11.76"));
    connection(HUD_EVENT_WIFI, HUD_CONNECTED, true);
    connection(HUD_EVENT_WIFI, HUD_DISCONNECTED, true);
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 Hz"));
    connection(HUD_EVENT_SOURCE, HUD_DISCONNECTED, true);
    assert(m.state == HUD_STATE_STREAMING && m.count == 2);
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    connection(HUD_EVENT_SOURCE, HUD_CONNECTED, true);
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    result("Right ankle", 13.12f);
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 Hz"));
    s = screen(5100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    /* Every indicator combination, lifecycle, and blink phase fits the display. */
    for (int dummy = 0; dummy <= 1; ++dummy)
        for (int w = HUD_DISCONNECTED; w <= HUD_CONNECTED; ++w)
            for (int n = HUD_DISCONNECTED; n <= HUD_CONNECTED; ++n)
                for (int state = HUD_STATE_BOOTING; state <= HUD_STATE_ERROR; ++state)
                    for (int blink = 0; blink <= 1; ++blink) {
                        m.state = state; m.wifi = w; m.source = n; m.dummy = dummy;
                        s = screen(100);
                        hud_model_screen(&m, 100, blink, &s);
                        assert((int)s.wifi == w && (int)s.source == n && s.dummy == dummy);
                        assert(s.blink == (blink && (w == HUD_CONNECTING || n == HUD_CONNECTING)));
                    }
    ready(false); event(HUD_EVENT_STREAM_OFFICIAL_STARTED); result("Left ankle", 1);
    connection(HUD_EVENT_WIFI, HUD_DISCONNECTED, false);
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    event(HUD_EVENT_BUTTON); assert(m.outgoing == HUD_CMD_NONE);
    /* Queue payload owns all string bytes: producer mutation cannot alter copy. */
    struct hud_event producer = {.type = HUD_EVENT_SYSTEM_INITIALIZED};
    memset(producer.session_name, 'A', sizeof(producer.session_name));
    struct hud_event queued = producer;
    memset(&producer, 0, sizeof(producer));
    assert(hud_model_apply(&m, &queued, 100) == 0);
    s = screen(100); assert(!strcmp(s.lines[1], "AAAAAAAAAAAA"));
    memset(&queued, 0, sizeof(queued));
    assert(m.session_name[0] == 'A');
    event(HUD_EVENT_STREAM_OFFICIAL_STARTED);
    result("Left ankle", 1);
    result("Right ankle", 2);
    struct hud_event third = {.type = HUD_EVENT_COMPUTE_RESULT, .location = "Third", .value = 3};
    assert(hud_model_apply(&m, &third, 100) == -ENOSPC && m.count == 2);
    third.value = NAN;
    assert(hud_model_apply(&m, &third, 100) == -EINVAL);
    result("Left ankle", 1e30f);
    s = screen(100); assert(!strcmp(s.lines[2], "Out of range"));
    const struct hud_event failure = {.type = HUD_EVENT_STREAM_STARTUP_FAILED};
    assert(hud_model_apply(&m, &failure, 100) == 0);
    s = screen(100); assert(!strcmp(s.lines[0], "ERROR") && !strcmp(s.lines[1], "Start failed"));
    puts("HUD model: transitions, requests, sensor selection, bounds, indicators, stale data and ownership passed");
}
