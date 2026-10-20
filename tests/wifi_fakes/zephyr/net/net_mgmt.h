#ifndef WIFI_TEST_NET_H
#define WIFI_TEST_NET_H
#include <stddef.h>
#include <stdint.h>
struct net_if { int unused; };
struct net_mgmt_event_callback {
    void *info;
    void (*handler)(struct net_mgmt_event_callback *, uint64_t, struct net_if *);
};
int net_mgmt(int request, struct net_if *iface, void *data, size_t size);
static inline void net_mgmt_init_event_callback(struct net_mgmt_event_callback *c,
    void (*h)(struct net_mgmt_event_callback *, uint64_t, struct net_if *), uint64_t mask)
{ (void)mask; c->handler = h; }
static inline void net_mgmt_add_event_callback(struct net_mgmt_event_callback *c) { (void)c; }
#endif
