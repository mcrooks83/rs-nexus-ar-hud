#include <assert.h>
#include <stdio.h>
#include "../src/vibration/vibration_manager.c"
int64_t test_now;
int test_output, test_pin;
int main(void)
{
    assert(vibration_manager_init()==0);
    assert(vibration_manager_pulse(0)==-EINVAL && !vibration_stop_work.scheduled);
    assert(vibration_manager_pulse(50)==0 && test_output==1);
    assert(vibration_stop_work.due==50);
    test_now=25;
    assert(vibration_manager_pulse(50)==0 && test_output==1);
    assert(vibration_stop_work.due==75); // one rescheduled stop, no pulse backlog
    test_now=75;
    vibration_stop_work.scheduled=false;
    vibration_stop_work.handler(&vibration_stop_work.work);
    assert(test_output==0 && !vibration_stop_work.scheduled);
    puts("Vibration: nonblocking 50 ms pulse, merged simultaneous pulses and single stop passed");
}
