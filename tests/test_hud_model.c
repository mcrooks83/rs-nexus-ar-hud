#include "hud_state_machine.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct hud_model m;
static void apply(struct hud_event e)
{ assert(hud_model_apply(&m, &e, 100) == 0); assert(m.outgoing == HUD_CMD_NONE); }
static void event(enum hud_event_type type) { apply((struct hud_event){.type = type}); }
static struct hud_screen screen(int64_t now)
{
    struct hud_screen s;
    hud_model_screen(&m, now, true, &s);
    for (unsigned i = 0; i < 3; ++i) assert(strlen(s.lines[i]) <= HUD_COLS);
    return s;
}
int main(void)
{
    hud_model_init(&m, false);
    struct hud_screen s = screen(0); assert(!strcmp(s.lines[0], "NEXUS HUD"));
    apply((struct hud_event){.type=HUD_EVENT_SOURCE,.connection=HUD_CONNECTED});
    apply((struct hud_event){.type=HUD_EVENT_WIFI,.connection=HUD_CONNECTED});
    s = screen(100); assert(!strcmp(s.lines[0], "Waiting for"));
    apply((struct hud_event){.type=HUD_EVENT_SYSTEM_INITIALIZED,.subject_id="subject-01",.session_name="Ankle session long"});
    s = screen(100); assert(!strcmp(s.lines[0], "Ready"));
    struct hud_event sensors = {.type=HUD_EVENT_SENSOR_CONNECTED,.subject_id="subject-01",.sensor_count=2,
        .sensors={{.address="left",.location="LEFT_ANKLE"},{.address="right",.location="RIGHT_ANKLE"}}};
    apply(sensors); assert(m.count == 2 && !m.pulse);
    s = screen(100); assert(!strcmp(s.lines[2], "No events"));
    event(HUD_EVENT_BUTTON); assert(m.selected == 1 && !m.pulse);
    s = screen(100); assert(!strcmp(s.lines[0], "Right ankle") && !strcmp(s.lines[2], "No events"));
    event(HUD_EVENT_STREAM_WARMUP_STARTED);
    s = screen(100); assert(!strcmp(s.lines[0], "Warming up"));
    event(HUD_EVENT_BUTTON); s = screen(100); assert(!strcmp(s.lines[2], "No events"));
    event(HUD_EVENT_STREAM_OFFICIAL_STARTED);
    struct hud_event result = {.type=HUD_EVENT_COMPUTE_RESULT,.subject_id="subject-01",.address="right",.value=0};
    apply(result); assert(m.pulse && m.selected == 0 && m.results[1].has_value);
    s = screen(100); assert(!strcmp(s.lines[2], "No events"));
    event(HUD_EVENT_BUTTON); s = screen(100); assert(!strcmp(s.lines[2], "0.00"));
    apply(sensors); assert(m.results[1].has_value && m.selected == 1 && !m.pulse);
    apply((struct hud_event){.type=HUD_EVENT_SOURCE,.connection=HUD_DISCONNECTED});
    s = screen(100); assert(m.count == 2 && m.state == HUD_STATE_STREAMING && !strcmp(s.lines[1], "LI 0-6 STALE"));
    apply((struct hud_event){.type=HUD_EVENT_SOURCE,.connection=HUD_CONNECTED});
    s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    apply(result); s = screen(100); assert(!strcmp(s.lines[1], "LI 0-6 Hz"));
    s = screen(5100); assert(!strcmp(s.lines[1], "LI 0-6 Hz"));
    s = screen(15099); assert(!strcmp(s.lines[1], "LI 0-6 Hz"));
    s = screen(15100); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    /* An updated value is still immediately stale on Wi-Fi loss, regardless
     * of the longer age limit, and reconnect alone must not freshen it. */
    apply(result);
    apply((struct hud_event){.type=HUD_EVENT_WIFI,.connection=HUD_DISCONNECTED});
    s = screen(101); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    apply((struct hud_event){.type=HUD_EVENT_WIFI,.connection=HUD_CONNECTED});
    s = screen(101); assert(!strcmp(s.lines[1], "LI 0-6 STALE"));
    apply(result);
    result.value = 1e30f; apply(result);
    s = screen(100); assert(!strcmp(s.lines[2], "1e+30"));
    event(HUD_EVENT_STREAM_STOPPED); s = screen(100); assert(!strcmp(s.lines[0], "Finalizing"));
    unsigned selected = m.selected;
    event(HUD_EVENT_BUTTON); event(HUD_EVENT_DOUBLE_BUTTON);
    assert(m.selected == selected && !m.inspect_sensor && m.state == HUD_STATE_DRAINING);
    s = screen(100); assert(!strcmp(s.lines[0], "Finalizing") && !s.lines[2][0]);
    m.inspect_sensor = true; /* Renderer must also prioritize a protected state. */
    s = screen(100); assert(!strcmp(s.lines[0], "Finalizing"));
    apply((struct hud_event){.type=HUD_EVENT_ERROR,.message="Sensor failed"});
    event(HUD_EVENT_BUTTON); event(HUD_EVENT_DOUBLE_BUTTON);
    assert(m.selected == selected && !m.inspect_sensor && m.state == HUD_STATE_ERROR);
    s = screen(100); assert(!strcmp(s.lines[0], "ERROR") && !strcmp(s.lines[1], "Sensor faile"));
    assert(!s.lines[2][0]);
    m.inspect_sensor = true;
    s = screen(100); assert(!strcmp(s.lines[0], "ERROR"));
    event(HUD_EVENT_STREAM_DRAINED); assert(m.state == HUD_STATE_READY);
    event(HUD_EVENT_BUTTON); assert(m.selected == (selected + 1) % 2);
    event(HUD_EVENT_DOUBLE_BUTTON); assert(m.selected == selected);
    event(HUD_EVENT_STREAM_OFFICIAL_STARTED);
    event(HUD_EVENT_BUTTON); assert(m.selected == (selected + 1) % 2);
    event(HUD_EVENT_DOUBLE_BUTTON); assert(m.selected == selected);
    for (int dummy=0; dummy<=1; ++dummy)
        for (int w=HUD_DISCONNECTED; w<=HUD_CONNECTED; ++w)
            for (int n=HUD_DISCONNECTED; n<=HUD_CONNECTED; ++n)
                for (int state=HUD_STATE_BOOTING; state<=HUD_STATE_ERROR; ++state)
                    for (int blink=0; blink<=1; ++blink) {
                        m.state=state; m.wifi=w; m.source=n; m.dummy=dummy;
                        s=screen(100); hud_model_screen(&m,100,blink,&s);
                        assert((int)s.wifi==w && (int)s.source==n && s.dummy==dummy);
                        assert(s.blink==(blink && (w==HUD_CONNECTING || n==HUD_CONNECTING)));
                        if (dummy) assert(!strncmp(s.lines[0],"Demo ",5));
                    }
    puts("HUD model: sensor views, lifecycle, stale data, disabled commands and text/indicator bounds passed");
}
