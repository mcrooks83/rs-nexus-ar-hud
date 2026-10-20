#ifndef WIFI_TEST_ATOMIC_H
#define WIFI_TEST_ATOMIC_H
#include <stdbool.h>
typedef int atomic_t;
#define ATOMIC_INIT(x) (x)
static inline int atomic_get(atomic_t *p) { return *p; }
static inline int atomic_set(atomic_t *p, int x) { int old = *p; *p = x; return old; }
static inline bool atomic_cas(atomic_t *p, int old, int x)
{ if (*p != old) return false; *p = x; return true; }
#endif
