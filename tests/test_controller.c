#include <assert.h>
#include <stdio.h>
#include "../src/state_machine/hud_state_machine.c"
int64_t test_now;
unsigned test_threads;
static unsigned pulses;
int display_manager_render(const struct hud_screen *s) { (void)s; return 0; }
int vibration_manager_pulse(uint32_t ms) { assert(ms==50); ++pulses; return 0; }
static int forbidden_command(enum hud_command command) { (void)command; assert(0); return 0; }
static void deliver(struct hud_event e)
{
    assert(hud_state_machine_handle_event(&e)==0);
    memset(&e,0,sizeof(e));
    struct hud_event queued;
    assert(k_msgq_get(&hud_events,&queued,K_NO_WAIT)==0);
    process_event(&queued);
}
int main(void)
{
    assert(hud_state_machine_init(false,forbidden_command)==0);
    deliver((struct hud_event){.type=HUD_EVENT_SYSTEM_INITIALIZED,.subject_id="s"});
    deliver((struct hud_event){.type=HUD_EVENT_SENSOR_CONNECTED,.subject_id="s",.sensor_count=2,
        .sensors={{.address="left",.location="LEFT_ANKLE"},{.address="right",.location="RIGHT_ANKLE"}}});
    assert(model.count==2 && pulses==0);
    deliver((struct hud_event){.type=HUD_EVENT_STREAM_WARMUP_STARTED});
    deliver((struct hud_event){.type=HUD_EVENT_STREAM_OFFICIAL_STARTED});
    deliver((struct hud_event){.type=HUD_EVENT_BUTTON});
    assert(model.selected==1 && pulses==0);
    struct hud_event result={.type=HUD_EVENT_COMPUTE_RESULT,.subject_id="s",.address="left",.value=12.48f};
    deliver(result); assert(pulses==1 && model.selected==1);
    strcpy(result.address,"right"); result.value=0;
    deliver(result); assert(pulses==2);
    strcpy(result.address,"unknown"); deliver(result); assert(pulses==2);
    strcpy(result.address,"left"); strcpy(result.subject_id,"other");
    deliver(result); assert(pulses==2);
    deliver((struct hud_event){.type=HUD_EVENT_SOURCE,.connection=HUD_DISCONNECTED});
    deliver((struct hud_event){.type=HUD_EVENT_BUTTON});
    deliver((struct hud_event){.type=HUD_EVENT_DOUBLE_BUTTON});
    deliver((struct hud_event){.type=HUD_EVENT_STREAM_STOPPED});
    deliver((struct hud_event){.type=HUD_EVENT_STREAM_DRAINED});
    assert(pulses==2 && model.count==2 && model.results[0].stale && model.results[1].stale);
    puts("HUD controller: copied queue payloads, 50 ms pulses for both slots, no command transmission passed");
}
