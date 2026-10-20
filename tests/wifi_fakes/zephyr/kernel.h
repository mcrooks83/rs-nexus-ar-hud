#ifndef WIFI_TEST_KERNEL_H
#define WIFI_TEST_KERNEL_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#define ARG_UNUSED(x) (void)(x)
#define IS_ENABLED(x) (x)
#define BUILD_ASSERT(c, msg) _Static_assert(c, msg)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define K_MSEC(x) (x)
#define K_FOREVER (-1)
#define K_NO_WAIT 0
#define K_THREAD_STACK_DEFINE(n,s) unsigned char n[s]
#define K_THREAD_STACK_SIZEOF(n) sizeof(n)
#define K_SEM_DEFINE(n,initial,maximum) struct k_sem n = {initial}
typedef int64_t k_timeout_t;
struct k_sem { int count; };
struct k_thread { int unused; };
extern int64_t test_now;
extern unsigned test_threads;
static inline int64_t k_uptime_get(void) { return test_now; }
static inline void k_sem_give(struct k_sem *s) { s->count = 1; }
static inline void k_sem_reset(struct k_sem *s) { s->count = 0; }
static inline int k_sem_take(struct k_sem *s, k_timeout_t t)
{ (void)t; if (!s->count) return -EAGAIN; s->count = 0; return 0; }
static inline void k_thread_create(struct k_thread *t, void *stack, size_t size,
    void (*entry)(void *, void *, void *), void *a, void *b, void *c,
    int priority, int options, k_timeout_t delay)
{
    (void)t; (void)stack; (void)size; (void)entry; (void)a; (void)b; (void)c;
    (void)priority; (void)options; (void)delay; ++test_threads;
}
static inline void k_thread_name_set(struct k_thread *t, const char *n)
{ (void)t; (void)n; }
#endif
