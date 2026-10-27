#include "station_router.h"
#include <string.h>
#include "access_point.h"
#include "gateway_link.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_netif_net_stack.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#define SUPERVISOR_INTERVAL_MS 50
#define SETUP_RETRY_US 1000000
#define DIAGNOSTIC_INTERVAL_US 60000000
static const char *TAG = "station_router";

static gw_access_state_t bridge_state(gw_status_t *status)
{
    if (!access_point_running() || !gateway_link_get_remote_status(status))
    {
        return GW_ACCESS_OFFLINE;
    }
    gw_access_network_t network = access_point_network();
    return gw_access_bridge_state(status, &network);
}

/* tcpip_thread only. Recheck B's most recent status and the real interface
 * state for every packet, including packets queued before a loss event. */
static bool route_ready(void)
{
    gw_status_t status;
    if (!gateway_link_data_enabled() || bridge_state(&status) != GW_ACCESS_READY)
    {
        return false;
    }
    struct netif *ap = esp_netif_get_netif_impl(access_point_netif());
    struct netif *spi = esp_netif_get_netif_impl(gateway_link_netif());
    return netif_is_up(ap) && netif_is_link_up(ap) && netif_is_up(spi) && netif_is_link_up(spi);
}

int station_ip4_input(struct pbuf *packet, struct netif *input)
{
    esp_netif_t *ap = access_point_netif();
    esp_netif_t *spi = gateway_link_netif();
    bool from_clients = ap && input == esp_netif_get_netif_impl(ap);
    bool from_bridge = spi && input == esp_netif_get_netif_impl(spi);
    if (!from_clients && !from_bridge)
    {
        return 0;
    }
    gw_access_network_t network = access_point_network();
    if (!gw_access_ip4_allowed(packet->payload, packet->len, packet->tot_len, &network,
                               from_clients, route_ready()))
    {
        pbuf_free(packet);
        return 1;
    }
    return 0;
}

int station_ip4_canforward(struct pbuf *packet, unsigned int destination)
{
    (void)packet;
    (void)destination;
    return route_ready() ? -1 : 0; /* -1 preserves lwIP's normal destination restrictions. */
}

/* The AP stays up for DHCP. Only the SPI data interface is disconnected.
 * If Wi-Fi stopped unexpectedly, defer NAT cleanup until its netif is up
 * again: the SDK cannot disable NAPT on a down netif. Never reopen first. */
static void stop_forwarding(bool *nat_enabled)
{
    gateway_link_close_gate();
    esp_netif_t *ap = access_point_netif();
    if (*nat_enabled && ap && esp_netif_is_netif_up(ap))
    {
        /* The AP can stop between the check and the call on tcpip_thread.
         * Retain ownership and retry cleanup before any future bring-up. */
        if (esp_netif_napt_disable(ap) == ESP_OK)
        {
            *nat_enabled = false;
            ESP_LOGI(TAG, "Client NAT mappings cleared");
        }
    }
    gateway_link_set_netif_up(false);
}

static void start_forwarding(bool *nat_enabled)
{
    gateway_link_set_netif_up(true);
    esp_err_t error = esp_netif_napt_enable(access_point_netif());
    if (error != ESP_OK)
    {
        gateway_link_set_netif_up(false);
        ESP_LOGE(TAG, "Cannot enable client NAPT: %s", esp_err_to_name(error));
        return;
    }
    *nat_enabled = true;
    if (!gateway_link_open_gate())
    {
        stop_forwarding(nat_enabled);
        return;
    }
    ESP_LOGI(TAG, "Client IPv4 forwarding enabled through Board B (reachability not probed)");
}

static const char *state_description(gw_access_state_t state)
{
    switch (state)
    {
    case GW_ACCESS_READY:
        return "Board B ready";
    case GW_ACCESS_ADDRESS_MISMATCH:
        return "SPI address settings disagree";
    case GW_ACCESS_SUBNET_OVERLAP:
        return "AP, SPI and upstream subnets must not overlap";
    default:
        return "AP or Board B unavailable; client forwarding closed";
    }
}

static void router_supervisor(void *argument)
{
    (void)argument;
    bool nat_enabled = false;
    uint32_t current_dns[2] = {0};
    int previous_state = -1;
    int64_t retry_after = 0;
    int64_t next_diagnostic = esp_timer_get_time() + DIAGNOSTIC_INTERVAL_US;
    for (;;)
    {
        gw_status_t status;
        gw_access_state_t state = bridge_state(&status);
        if ((int)state != previous_state)
        {
            ESP_LOGI(TAG, "%s", state_description(state));
            previous_state = state;
        }
        uint32_t dns[2] = {0};
        bool wanted = state == GW_ACCESS_READY;
        if (wanted)
        {
            gw_access_network_t network = access_point_network();
            gw_access_dns(&status, &network, dns);
            dns[0] = htonl(dns[0]);
            dns[1] = htonl(dns[1]);
        }
        bool dns_changed = wanted && memcmp(dns, current_dns, sizeof(dns));
        if (nat_enabled && (!wanted || !gateway_link_data_enabled() || dns_changed))
        {
            stop_forwarding(&nat_enabled);
        }
        if (wanted && !nat_enabled && esp_timer_get_time() >= retry_after)
        {
            /* DNS changes intentionally refresh leases before client traffic resumes. */
            esp_err_t error = access_point_set_dns(dns);
            if (error == ESP_OK)
            {
                memcpy(current_dns, dns, sizeof(dns));
                start_forwarding(&nat_enabled);
            }
            else
            {
                ESP_LOGW(TAG, "DHCP DNS update failed: %s", esp_err_to_name(error));
            }
            retry_after = esp_timer_get_time() + SETUP_RETRY_US;
        }
        if (esp_timer_get_time() >= next_diagnostic)
        {
            ESP_LOGI(TAG, "Internal heap free=%u minimum=%u; forwarding=%s",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                     gateway_link_data_enabled() ? "enabled" : "closed");
            next_diagnostic = esp_timer_get_time() + DIAGNOSTIC_INTERVAL_US;
        }
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_INTERVAL_MS));
    }
}

esp_err_t station_router_start(void)
{
    return xTaskCreate(router_supervisor, "station_router", 4096, NULL, 6, NULL) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
