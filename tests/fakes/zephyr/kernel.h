#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
#include <stdbool.h>
#include <stdint.h>
#define ARG_UNUSED(x) (void)(x)
#define K_MSEC(x) (x)
#define BIT(x) (1U << (x))
struct k_work { int unused; };
struct k_work_delayable {
    struct k_work work;
    void (*handler)(struct k_work *);
    int64_t due;
    bool scheduled;
};
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.handler = fn}
extern int64_t test_now;
static inline int64_t k_uptime_get(void) { return test_now; }
static inline int k_work_reschedule(struct k_work_delayable *w, int delay)
{ w->due = test_now + delay; w->scheduled = true; return 0; }
static inline int k_work_cancel_delayable(struct k_work_delayable *w)
{ w->scheduled = false; return 0; }
#endif
