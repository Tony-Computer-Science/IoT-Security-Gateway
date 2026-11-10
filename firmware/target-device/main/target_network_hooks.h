#pragma once
#include <stdbool.h>
struct pbuf;
struct netif;
bool target_network_init(void);
int target_ip4_input(struct pbuf *packet, struct netif *input);
#define LWIP_HOOK_IP4_INPUT(p, n) target_ip4_input(p, n)
