#include "net_mgmt.h"
#define NET_ADDR_PREFERRED 1
struct net_if *net_if_get_wifi_sta(void);
struct net_if *net_if_get_first_wifi(void);
void *net_if_ipv4_get_global_addr(struct net_if *iface, int state);
int net_if_up(struct net_if *iface);
