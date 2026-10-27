#include "access_point.h"
#include <string.h>
#include "sdkconfig.h"
#include "gateway_link.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "dhcpserver/dhcpserver.h"

static const char *TAG = "access_point";
static esp_netif_t *ap_netif;
static gw_access_network_t network;
static portMUX_TYPE lifecycle_lock = portMUX_INITIALIZER_UNLOCKED;
static bool running;
/* Only the router supervisor changes DNS after initialization. */
static uint32_t advertised_dns[2];

esp_netif_t *access_point_netif(void)
{
    return ap_netif;
}

gw_access_network_t access_point_network(void)
{
    return network;
}

bool access_point_running(void)
{
    portENTER_CRITICAL(&lifecycle_lock);
    bool value = running;
    portEXIT_CRITICAL(&lifecycle_lock);
    return value;
}

static esp_err_t start_dhcp(void)
{
    esp_err_t error = esp_netif_dhcps_start(ap_netif);
    return error == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED ? ESP_OK : error;
}

static esp_err_t stop_dhcp(void)
{
    esp_err_t error = esp_netif_dhcps_stop(ap_netif);
    return error == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED ? ESP_OK : error;
}

static void on_wifi_event(void *argument, esp_event_base_t base, int32_t event_id, void *data)
{
    (void)argument;
    (void)base;
    (void)data;
    if (event_id == WIFI_EVENT_AP_START || event_id == WIFI_EVENT_AP_STOP)
    {
        bool started = event_id == WIFI_EVENT_AP_START;
        if (started)
        {
            ESP_ERROR_CHECK(start_dhcp());
        }
        portENTER_CRITICAL(&lifecycle_lock);
        running = started;
        portEXIT_CRITICAL(&lifecycle_lock);
        if (!started)
        {
            gateway_link_request_reset();
        }
        ESP_LOGI(TAG, "WPA2 AP %s; DHCP %s", started ? "started" : "stopped",
                 started ? "available" : "unavailable");
    }
    else if (event_id == WIFI_EVENT_AP_STACONNECTED || event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "Client %s", event_id == WIFI_EVENT_AP_STACONNECTED ? "associated" : "left");
    }
}

esp_err_t access_point_set_dns(const uint32_t servers[2])
{
    if (!ap_netif || !access_point_running())
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!memcmp(servers, advertised_dns, sizeof(advertised_dns)))
    {
        return ESP_OK;
    }
    esp_err_t error = stop_dhcp();
    if (error != ESP_OK)
    {
        return error;
    }
    for (unsigned i = 0; i < 2 && error == ESP_OK; ++i)
    {
        esp_netif_dns_info_t dns = {.ip.type = ESP_IPADDR_TYPE_V4};
        dns.ip.u_addr.ip4.addr = servers[i];
        error =
            esp_netif_set_dns_info(ap_netif, i ? ESP_NETIF_DNS_BACKUP : ESP_NETIF_DNS_MAIN, &dns);
    }
    uint8_t offer_dns = servers[0] != 0;
    if (error == ESP_OK)
    {
        error = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                       &offer_dns, sizeof(offer_dns));
    }
    /* Restore DHCP even if an option failed. The supervisor will retry. */
    esp_err_t restart_error = start_dhcp();
    if (error != ESP_OK || restart_error != ESP_OK)
    {
        return error != ESP_OK ? error : restart_error;
    }
    ESP_LOGI(TAG, "DNS options changed; reconnecting clients to refresh DHCP");
    error = esp_wifi_deauth_sta(0); /* All associated clients, not an AP restart. */
    if (error == ESP_OK)
    {
        memcpy(advertised_dns, servers, sizeof(advertised_dns));
    }
    return error;
}

esp_err_t access_point_start(void)
{
    const char *ssid = CONFIG_GW_AP_SSID;
    const char *password = CONFIG_GW_AP_PASSWORD;
    esp_ip4_addr_t ap_ip;
    esp_netif_ip_info_t spi = gateway_link_addresses();
    if (esp_netif_str_to_ip4(CONFIG_GW_AP_IP, &ap_ip) != ESP_OK)
    {
        return ESP_ERR_INVALID_ARG;
    }
    network = (gw_access_network_t){.ap_ip = ntohl(ap_ip.addr),
                                    .spi_ip = ntohl(spi.ip.addr),
                                    .spi_peer = ntohl(gateway_link_peer_ip()),
                                    .spi_mask = ntohl(spi.netmask.addr)};
    if (!strlen(ssid) || strlen(ssid) > 32 || strlen(password) < 8 || strlen(password) > 63 ||
        !gw_access_config_valid(&network))
    {
        ESP_LOGE(TAG, "Configure SSID, 8-63 byte passphrase and a private .1/24 distinct from SPI");
        return ESP_ERR_INVALID_ARG;
    }
    if (ap_netif)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif)
    {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(stop_dhcp());
    esp_netif_ip_info_t ip = {.ip = ap_ip, .gw = ap_ip, .netmask.addr = htonl(GW_AP_MASK)};
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
    dhcps_lease_t pool = {.enable = true,
                          .start_ip.addr = htonl((network.ap_ip & GW_AP_MASK) | 2),
                          .end_ip.addr = htonl((network.ap_ip & GW_AP_MASK) | 100)};
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
                                           ESP_NETIF_REQUESTED_IP_ADDRESS, &pool, sizeof(pool)));
    uint32_t minutes = CONFIG_GW_AP_LEASE_MINUTES;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(
        ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &minutes, sizeof(minutes)));
    uint8_t offer_dns = 0;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &offer_dns, sizeof(offer_dns)));
    /* AP-start defaults run before our handler, which then starts DHCP. */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initialization));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, ssid, strlen(ssid));
    memcpy(config.ap.password, password, strlen(password));
    config.ap.ssid_len = strlen(ssid);
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    config.ap.channel = CONFIG_GW_AP_CHANNEL;
    config.ap.max_connection = CONFIG_GW_AP_MAX_CLIENTS;
    config.ap.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Gateway " IPSTR "/24, DHCP .2-.100, channel=%d, clients=%d", IP2STR(&ap_ip),
             CONFIG_GW_AP_CHANNEL, CONFIG_GW_AP_MAX_CLIENTS);
    return ESP_OK;
}
