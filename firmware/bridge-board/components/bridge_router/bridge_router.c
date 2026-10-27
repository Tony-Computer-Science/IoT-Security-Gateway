#include "bridge_router.h"
#include <string.h>
#include "gateway_link.h"
#include "uplink_wifi.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ip4.h"

static const char *TAG = "bridge_router";
#define SUPERVISOR_INTERVAL_MS 50

/* tcpip_thread only: default Wi-Fi handlers may change the interface before
 * our event handler runs. Do not use an old lease/NAT during that interval. */
static bool route_ready(void)
{
    esp_netif_t *wan = uplink_wifi_netif();
    if (!wan || !gateway_link_data_enabled())
    {
        return false;
    }
    struct netif *upstream = esp_netif_get_netif_impl(wan);
    gw_status_t status;
    uplink_wifi_status(&status);
    return (status.flags & GW_IPV4) && netif_is_up(upstream) && netif_is_link_up(upstream) &&
           !memcmp(status.ip, &netif_ip4_addr(upstream)->addr, 4) &&
           !memcmp(status.mask, &netif_ip4_netmask(upstream)->addr, 4) &&
           !memcmp(status.gateway, &netif_ip4_gw(upstream)->addr, 4);
}
/* Called by lwIP before NAT. Prevent direct upstream routing into the SPI
 * subnet, even if a user on the upstream LAN manually installs such a route. */
int gateway_ip4_input(struct pbuf *packet, struct netif *input)
{
    esp_netif_t *spi = gateway_link_netif();
    esp_netif_t *wan = uplink_wifi_netif();
    if (!spi)
    {
        return 0;
    }
    bool from_spi = input == esp_netif_get_netif_impl(spi);
    bool from_wan = wan && input == esp_netif_get_netif_impl(wan);
    esp_netif_ip_info_t link_ip = gateway_link_addresses();
    gw_ipv4_policy_t policy = {
        .ingress = from_spi   ? GW_INGRESS_SPI
                   : from_wan ? GW_INGRESS_UPLINK
                              : GW_INGRESS_LOCAL,
        .forwarding_ready = route_ready(),
        .peer_ipv4 = ntohl(gateway_link_peer_ip()),
        .link_ipv4 = ntohl(link_ip.ip.addr),
        .link_mask = ntohl(link_ip.netmask.addr),
    };
    bool reject = !gw_ip4_allowed(packet->payload, packet->len, packet->tot_len, &policy);
    if (reject)
    {
        pbuf_free(packet);
        return 1;
    }
    return 0;
}
int gateway_ip4_canforward(struct pbuf *packet, unsigned int destination)
{
    (void)packet;
    (void)destination;
    /* Checked in tcpip_thread for queued input too, not only on SPI enqueue. */
    if (!route_ready())
    {
        return 0;
    }
    return -1; /* retain lwIP's normal broadcast/multicast/link-local rules */
}
/* Only this component changes B's netif/NAPT lifecycle. Disable while the
 * interface is still up: ESP-IDF otherwise cannot clear that NAPT instance. */
static void stop_forwarding(void)
{
    gateway_link_close_gate();
    ESP_ERROR_CHECK(esp_netif_napt_disable(gateway_link_netif()));
    gateway_link_set_netif_up(false);
    ESP_LOGW(TAG, "Forwarding stopped; NAT mappings cleared");
}

static bool start_forwarding(void)
{
    gateway_link_set_netif_up(true);
    esp_err_t error = esp_netif_napt_enable(gateway_link_netif());
    if (error != ESP_OK)
    {
        gateway_link_set_netif_up(false);
        ESP_LOGE(TAG, "Cannot enable NAPT: %s", esp_err_to_name(error));
        return false;
    }
    if (!gateway_link_open_gate())
    {
        /* Pairing may have changed while ESP-NETIF calls ran on tcpip_thread. */
        stop_forwarding();
        return false;
    }
    ESP_LOGI(TAG, "IPv4 forwarding enabled");
    return true;
}

static void router_supervisor(void *arg)
{
    (void)arg;
    bool active = false;
    for (;;)
    {
        gw_status_t status;
        uplink_wifi_status(&status);
        /* DHCP can replace DNS without changing the IP lease or posting a new
         * GOT_IP event. Publish current resolver options to Board A as well. */
        if (status.flags & GW_IPV4)
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                esp_netif_dns_info_t dns;
                if (esp_netif_get_dns_info(uplink_wifi_netif(),
                                           i ? ESP_NETIF_DNS_BACKUP : ESP_NETIF_DNS_MAIN,
                                           &dns) == ESP_OK &&
                    dns.ip.type == ESP_IPADDR_TYPE_V4)
                {
                    memcpy(status.dns[i], &dns.ip.u_addr.ip4.addr, 4);
                }
            }
        }
        esp_netif_ip_info_t ip = gateway_link_addresses();
        uint32_t upstream, mask, peer = gateway_link_peer_ip();
        memcpy(&upstream, status.ip, 4);
        memcpy(&mask, status.mask, 4);
        bool overlap = (status.flags & GW_IPV4) &&
                       gw_subnets_overlap(upstream, mask, ip.ip.addr, ip.netmask.addr);
        bool wanted = gw_can_forward(gateway_link_ready(), status.flags & GW_IPV4, overlap, false);
        /* The transport can close its gate between supervisor iterations. */
        if (active && (!wanted || !gateway_link_data_enabled()))
        {
            stop_forwarding();
            active = false;
        }
        if (!active && wanted)
        {
            active = start_forwarding();
        }
        if (active && gateway_link_data_enabled())
        {
            status.flags |= GW_FORWARDING;
        }
        if (overlap)
        {
            status.flags |= GW_OVERLAP;
        }
        memcpy(status.link_local, &ip.ip.addr, 4);
        memcpy(status.link_peer, &peer, 4);
        memcpy(status.link_mask, &ip.netmask.addr, 4);
        gateway_link_publish_status(&status);
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_INTERVAL_MS));
    }
}
esp_err_t bridge_router_start(void)
{
    return xTaskCreate(router_supervisor, "router", 4096, NULL, 6, NULL) == pdPASS ? ESP_OK
                                                                                   : ESP_ERR_NO_MEM;
}
