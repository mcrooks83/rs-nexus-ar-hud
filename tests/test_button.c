#include <assert.h>
#include <stdio.h>
#include "../src/button/button_manager.c"
int64_t test_now;
int test_pin;
static unsigned singles, doubles;
static void single(void) { ++singles; }
static void double_press(void) { ++doubles; }
static void advance(int64_t target)
{
    while (true) {
        struct k_work_delayable *next = NULL;
        if (debounce_work.scheduled && debounce_work.due <= target) next = &debounce_work;
        if (single_work.scheduled && single_work.due <= target &&
            (!next || single_work.due < next->due)) next = &single_work;
        if (!next) break;
        test_now = next->due; next->scheduled = false;
        next->handler(&next->work);
    }
    test_now = target;
}
static void edge(int64_t at, int active)
{
    advance(at); test_pin = active;
    button_interrupt_handler(NULL, NULL, 1);
}
int main(void)
{
    assert(button_manager_init(single, double_press) == 0);
    edge(10, 1); edge(15, 0); edge(20, 1); // bounce
    advance(60); assert(singles == 0 && doubles == 0);
    edge(100, 0); advance(399); assert(singles == 0);
    advance(400); assert(singles == 1 && doubles == 0);
    edge(1000, 1); edge(1080, 0); edge(1200, 1); edge(1280, 0);
    advance(1600); assert(singles == 1 && doubles == 1);
    // Holding and repeated active edges cannot become a double press.
    edge(2000, 1); edge(2100, 1); advance(2400);
    assert(singles == 2 && doubles == 1);
    edge(2500, 0); edge(2700, 1); edge(2800, 0);
    advance(3100); assert(singles == 3 && doubles == 1);
    puts("Button: debounce, deferred single, double cancellation and held press passed");
}
