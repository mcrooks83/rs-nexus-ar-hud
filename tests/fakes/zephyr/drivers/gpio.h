#ifndef TEST_GPIO_H
#define TEST_GPIO_H
#include <stdint.h>
#include <stdbool.h>
#define DT_ALIAS(x) 0
#define DT_NODE_HAS_STATUS(n, s) 1
#define GPIO_DT_SPEC_GET(n, prop) {0}
#define GPIO_INPUT 1
#define GPIO_INT_EDGE_BOTH 2
struct device { int unused; };
struct gpio_callback { int unused; };
struct gpio_dt_spec { struct device *port; unsigned pin; };
extern int test_pin;
static inline bool gpio_is_ready_dt(const struct gpio_dt_spec *s) { (void)s; return true; }
static inline int gpio_pin_get_dt(const struct gpio_dt_spec *s) { (void)s; return test_pin; }
static inline int gpio_pin_configure_dt(const struct gpio_dt_spec *s, int flags)
{ (void)s; (void)flags; return 0; }
static inline int gpio_pin_interrupt_configure_dt(const struct gpio_dt_spec *s, int flags)
{ (void)s; (void)flags; return 0; }
static inline void gpio_init_callback(struct gpio_callback *c,
    void (*fn)(const struct device *, struct gpio_callback *, uint32_t), unsigned pins)
{ (void)c; (void)fn; (void)pins; }
static inline int gpio_add_callback(struct device *d, struct gpio_callback *c)
{ (void)d; (void)c; return 0; }
#endif
