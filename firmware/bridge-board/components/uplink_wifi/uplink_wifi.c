/* Upstream association/DHCP lifecycle. All connection decisions run on the
 * default event loop; timer callbacks only post work to that loop. */
#include "uplink_wifi.h"
#include <string.h>
#include "sdkconfig.h"
#include "gateway_link.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#define INITIAL_RETRY_MS 1000u
#define MAX_RETRY_MS 30000u
#define DHCP_WAIT_US 30000000ULL
#define EVENT_QUEUE_RETRY_US 100000ULL

static const char *TAG = "uplink_wifi";
static esp_netif_t *uplink_netif;
static esp_timer_handle_t reconnect_timer;
static esp_timer_handle_t dhcp_timer;

/* Readers include the router supervisor and lwIP thread. Never hold this lock
 * while calling Wi-Fi, timers, ESP-NETIF, or the SPI component. */
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
static gw_status_t current_status;
static uint32_t next_retry_ms = INITIAL_RETRY_MS;

ESP_EVENT_DEFINE_BASE(UPLINK_CONTROL);
enum control_event
{
    RETRY_CONNECT,
    DHCP_TIMEOUT
};

esp_netif_t *uplink_wifi_netif(void)
{
    return uplink_netif;
}

void uplink_wifi_status(gw_status_t *status)
{
    portENTER_CRITICAL(&status_lock);
    *status = current_status;
    portEXIT_CRITICAL(&status_lock);
}

bool uplink_wifi_ready(void)
{
    gw_status_t status;
    uplink_wifi_status(&status);
    return (status.flags & GW_IPV4) != 0;
}

/* A full event queue must not block the ESP timer task or lose reconnection
 * permanently. Retry the notification shortly; stale events are harmless. */
static void post_control_event(int32_t event_id, esp_timer_handle_t timer)
{
    if (esp_event_post(UPLINK_CONTROL, event_id, NULL, 0, 0) != ESP_OK)
    {
        esp_timer_start_once(timer, EVENT_QUEUE_RETRY_US);
    }
}

static void on_reconnect_timer(void *argument)
{
    (void)argument;
    post_control_event(RETRY_CONNECT, reconnect_timer);
}

static void on_dhcp_timer(void *argument)
{
    (void)argument;
    post_control_event(DHCP_TIMEOUT, dhcp_timer);
}

static void schedule_reconnect(void)
{
    esp_timer_stop(reconnect_timer);

    portENTER_CRITICAL(&status_lock);
    uint32_t delay_ms = next_retry_ms;
    current_status.retry_ms = delay_ms;
    next_retry_ms = next_retry_ms < MAX_RETRY_MS / 2 ? next_retry_ms * 2 : MAX_RETRY_MS;
    portEXIT_CRITICAL(&status_lock);

    ESP_LOGI(TAG, "Retry in %lu ms", (unsigned long)delay_ms);
    ESP_ERROR_CHECK(esp_timer_start_once(reconnect_timer, (uint64_t)delay_ms * 1000));
}

static void attempt_connection(void)
{
    gw_status_t status;
    uplink_wifi_status(&status);
    if (status.flags & GW_ASSOCIATED)
    {
        return; /* A timer event was queued before association completed. */
    }

    esp_err_t error = esp_wifi_connect();
    if (error != ESP_OK)
    {
        ESP_LOGW(TAG, "Connection attempt failed: %s", esp_err_to_name(error));
        schedule_reconnect();
    }
}

static void handle_association(const wifi_event_sta_connected_t *event)
{
    /* The scan threshold alone cannot enforce the selected authentication mode. */
    wifi_auth_mode_t auth = event->authmode;
#if CONFIG_GW_UPLINK_OWE
    if (auth != WIFI_AUTH_OWE)
    {
        ESP_LOGW(TAG, "Rejected non-OWE authentication mode");
        esp_wifi_disconnect();
        return;
    }
#else
    if (auth != WIFI_AUTH_WPA2_PSK && auth != WIFI_AUTH_WPA3_PSK && auth != WIFI_AUTH_WPA2_WPA3_PSK)
    {
        ESP_LOGW(TAG, "Rejected non-personal/WPA2-WPA3 authentication mode");
        esp_wifi_disconnect();
        return;
    }
#endif

    portENTER_CRITICAL(&status_lock);
    current_status.flags |= GW_ASSOCIATED;
    portEXIT_CRITICAL(&status_lock);

    ESP_LOGI(TAG, "Associated; awaiting DHCP");
    esp_timer_stop(dhcp_timer);
    ESP_ERROR_CHECK(esp_timer_start_once(dhcp_timer, DHCP_WAIT_US));
}

static void handle_disconnect(const wifi_event_sta_disconnected_t *event)
{
    portENTER_CRITICAL(&status_lock);
    current_status.flags &= ~(GW_IPV4 | GW_ASSOCIATED);
    memset(current_status.ip, 0, sizeof(current_status.ip));
    memset(current_status.mask, 0, sizeof(current_status.mask));
    memset(current_status.gateway, 0, sizeof(current_status.gateway));
    memset(current_status.dns, 0, sizeof(current_status.dns));
    if (event)
    {
        current_status.reason = event->reason;
    }
    uint32_t reason = current_status.reason;
    portEXIT_CRITICAL(&status_lock);

    gateway_link_request_reset();
    esp_timer_stop(dhcp_timer);
    ESP_LOGW(TAG, "Upstream unavailable, reason=%lu", (unsigned long)reason);

    if (event)
    {
        schedule_reconnect();
    }
    else
    {
        /* Lost-IP can occur while still associated. Its disconnect event
         * will schedule the retry in the same serialized event loop. */
        esp_wifi_disconnect();
    }
}

static void handle_ipv4_acquired(const ip_event_got_ip_t *event)
{
    gw_status_t previous;
    uplink_wifi_status(&previous);
    if (!(previous.flags & GW_ASSOCIATED))
    {
        return; /* Ignore a lease for an association we rejected. */
    }

    esp_netif_dns_info_t dns[2] = {0};
    esp_netif_get_dns_info(uplink_netif, ESP_NETIF_DNS_MAIN, &dns[0]);
    esp_netif_get_dns_info(uplink_netif, ESP_NETIF_DNS_BACKUP, &dns[1]);

    bool changed = !(previous.flags & GW_IPV4) || memcmp(previous.ip, &event->ip_info.ip.addr, 4) ||
                   memcmp(previous.mask, &event->ip_info.netmask.addr, 4) ||
                   memcmp(previous.gateway, &event->ip_info.gw.addr, 4);
    if (changed)
    {
        gateway_link_request_reset();
    }

    portENTER_CRITICAL(&status_lock);
    current_status.flags |= GW_IPV4 | GW_ASSOCIATED;
    current_status.retry_ms = 0;
    next_retry_ms = INITIAL_RETRY_MS;
    memcpy(current_status.ip, &event->ip_info.ip.addr, 4);
    memcpy(current_status.mask, &event->ip_info.netmask.addr, 4);
    memcpy(current_status.gateway, &event->ip_info.gw.addr, 4);
    memcpy(current_status.dns[0], &dns[0].ip.u_addr.ip4.addr, 4);
    memcpy(current_status.dns[1], &dns[1].ip.u_addr.ip4.addr, 4);
    portEXIT_CRITICAL(&status_lock);

    esp_timer_stop(reconnect_timer);
    esp_timer_stop(dhcp_timer);
    ESP_LOGI(TAG, "IPv4 acquired: " IPSTR " (internet reachability not probed)",
             IP2STR(&event->ip_info.ip));
}

static void handle_uplink_event(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    if (base == UPLINK_CONTROL)
    {
        if (id == RETRY_CONNECT)
        {
            attempt_connection();
        }
        else if (id == DHCP_TIMEOUT && !uplink_wifi_ready())
        {
            ESP_LOGW(TAG, "DHCP timeout; reconnecting");
            esp_wifi_disconnect();
        }
    }
    else if (base == WIFI_EVENT)
    {
        switch (id)
        {
        case WIFI_EVENT_STA_START:
            attempt_connection();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            handle_association(data);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            handle_disconnect(data);
            break;
        default:
            break;
        }
    }
    else if (base == IP_EVENT)
    {
        if (id == IP_EVENT_STA_GOT_IP)
        {
            handle_ipv4_acquired(data);
        }
        else if (id == IP_EVENT_STA_LOST_IP)
        {
            handle_disconnect(NULL);
        }
    }
}

esp_err_t uplink_wifi_start(void)
{
    const char *ssid = CONFIG_GW_UPLINK_SSID;
    const char *password = CONFIG_GW_UPLINK_PASSWORD;
#if CONFIG_GW_UPLINK_OWE
    bool credentials_valid = strlen(password) == 0;
#else
    bool credentials_valid = strlen(password) >= 8 && strlen(password) <= 63;
#endif
    if (!strlen(ssid) || strlen(ssid) > 32 || !credentials_valid)
    {
        portENTER_CRITICAL(&status_lock);
        current_status.flags = GW_CONFIG_ERROR;
        portEXIT_CRITICAL(&status_lock);
        ESP_LOGE(TAG, "Set SSID and credentials for the selected upstream authentication mode");
        return ESP_ERR_INVALID_ARG;
    }

    uplink_netif = esp_netif_create_default_wifi_sta();
    if (!uplink_netif)
    {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(esp_netif_set_default_netif(uplink_netif));

    esp_timer_create_args_t timer = {.callback = on_reconnect_timer, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&timer, &reconnect_timer));
    timer.callback = on_dhcp_timer;
    timer.name = "dhcp_timeout";
    ESP_ERROR_CHECK(esp_timer_create(&timer, &dhcp_timer));

    ESP_ERROR_CHECK(
        esp_event_handler_register(UPLINK_CONTROL, ESP_EVENT_ANY_ID, handle_uplink_event, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, handle_uplink_event, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, handle_uplink_event, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, handle_uplink_event, NULL));

    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initialization));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, strlen(ssid));
    memcpy(config.sta.password, password, strlen(password));
#if CONFIG_GW_UPLINK_OWE
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.owe_enabled = true;
    config.sta.pmf_cfg.required = true;
#else
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.required = false;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
#endif
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    return ESP_OK;
}
