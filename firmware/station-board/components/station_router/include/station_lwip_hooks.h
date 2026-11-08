#pragma once
struct pbuf;
struct netif;
int station_ip4_input(struct pbuf *packet, struct netif *input);
int station_ip4_canforward(struct pbuf *packet, unsigned int destination);
#define LWIP_HOOK_IP4_INPUT(p, n) station_ip4_input(p, n)
#define LWIP_HOOK_IP4_CANFORWARD(p, d) station_ip4_canforward(p, d)
