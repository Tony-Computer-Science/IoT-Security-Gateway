#include "target_network_hooks.h"
#include "target_network_policy.h"
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/pbuf.h"

static uint32_t pc_ipv4;

bool target_network_init(void)
{
    ip4_addr_t pc;
    if (!ip4addr_aton(CONFIG_TARGET_UI_HOST, &pc) || !pc.addr)
    {
        return false;
    }
    pc_ipv4 = ntohl(pc.addr);
    return true;
}

int target_ip4_input(struct pbuf *packet, struct netif *input)
{
    (void)input;
    uint8_t header[68]; /* Maximum IPv4 plus UDP/ICMP header, across pbuf fragments. */
    size_t length = packet->tot_len < sizeof(header) ? packet->tot_len : sizeof(header);
    if (pc_ipv4 && pbuf_copy_partial(packet, header, length, 0) == length &&
        target_ipv4_allowed(header, length, pc_ipv4))
    {
        return 0;
    }
    pbuf_free(packet);
    return 1;
}
