#include "gateway_link.h"
#include <stddef.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_netif_defaults.h"
#include "esp_netif_net_stack.h"
#include "lwip/inet.h"

#define HS_GPIO 4
#define QUEUE_DEPTH 8
/* Slave trans_len saturates at configured length. A guard word lets us
 * distinguish a valid 1600-byte transfer from ANY oversized transaction. */
#define DMA_SIZE (GW_FRAME_SIZE + 4)
typedef struct
{
    uint32_t generation;
    uint16_t length;
    uint8_t data[GW_ETH_MAX];
} packet_t;
typedef struct
{
    esp_netif_driver_base_t base;
    esp_netif_t *netif;
    esp_netif_ip_info_t ip;
    uint32_t peer_ip;
    QueueHandle_t tx_queue;
    QueueHandle_t free_rx_buffers;
    packet_t rx_pool[QUEUE_DEPTH];
    gw_session_t session;
    gw_status_t status, remote;
    bool reset;
    bool gate;
    bool remote_valid;
    bool netif_up;
    uint32_t generation;
    uint32_t received;
    uint32_t rejected;
    uint32_t dropped;
    uint32_t tx_sequence;
    uint8_t *tx_dma;
    uint8_t *rx_dma;
} link_context_t;

/* state_lock protects session, gate, generation, reset, snapshots and counters.
 * The SPI task owns DMA buffers/sequence; the supervisor owns netif_up.
 * RTOS queues synchronize packet ownership independently of this lock. */
static link_context_t s_link;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static const char *TAG = "gateway_link";

static uint64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}
static uint64_t create_session_id(void)
{
    uint64_t id;
    do
    {
        esp_fill_random(&id, sizeof(id));
    } while (!id);
    return id;
}
static void record_packet_drop(void)
{
    portENTER_CRITICAL(&state_lock);
    ++s_link.dropped;
    portEXIT_CRITICAL(&state_lock);
}
void gateway_link_close_gate(void)
{
    portENTER_CRITICAL(&state_lock);
    s_link.gate = false;
    ++s_link.generation;
    portEXIT_CRITICAL(&state_lock);
}
void gateway_link_request_reset(void)
{
    portENTER_CRITICAL(&state_lock);
    s_link.gate = false;
    ++s_link.generation;
    s_link.reset = true;
    s_link.remote_valid = false;
    portEXIT_CRITICAL(&state_lock);
}
bool gateway_link_ready(void)
{
    portENTER_CRITICAL(&state_lock);
    bool ready = s_link.session.ready && !s_link.reset;
    portEXIT_CRITICAL(&state_lock);
    return ready;
}
bool gateway_link_data_enabled(void)
{
    portENTER_CRITICAL(&state_lock);
    bool enabled = s_link.gate && s_link.session.ready && !s_link.reset;
    portEXIT_CRITICAL(&state_lock);
    return enabled;
}
esp_netif_t *gateway_link_netif(void)
{
    return s_link.netif;
}
esp_netif_ip_info_t gateway_link_addresses(void)
{
    return s_link.ip;
}
uint32_t gateway_link_peer_ip(void)
{
    return s_link.peer_ip;
}

void gateway_link_set_netif_up(bool up)
{
    if (!up)
    {
        gateway_link_close_gate();
        xQueueReset(s_link.tx_queue);
        if (s_link.netif_up)
        {
            esp_netif_action_disconnected(s_link.netif, NULL, 0, NULL);
        }
        s_link.netif_up = false;
        return;
    }
    if (!s_link.netif_up)
    {
        esp_netif_action_connected(s_link.netif, NULL, 0, NULL);
        s_link.netif_up = true;
    }
}
bool gateway_link_open_gate(void)
{
    portENTER_CRITICAL(&state_lock);
    s_link.gate = s_link.session.ready && !s_link.reset;
    bool open = s_link.gate;
    portEXIT_CRITICAL(&state_lock);
    return open;
}
void gateway_link_publish_status(const gw_status_t *status)
{
    portENTER_CRITICAL(&state_lock);
    s_link.status = *status;
    portEXIT_CRITICAL(&state_lock);
}
bool gateway_link_get_remote_status(gw_status_t *status)
{
    portENTER_CRITICAL(&state_lock);
    bool valid = s_link.remote_valid && s_link.session.ready && !s_link.reset;
    *status = s_link.remote;
    portEXIT_CRITICAL(&state_lock);
    return valid;
}
static esp_err_t enqueue_tx_packet(void *handle, void *buffer, size_t length)
{
    (void)handle;
    if (!gw_ethernet_valid(buffer, length))
    {
        return ESP_ERR_INVALID_ARG;
    }
    packet_t packet = {.length = length};
    portENTER_CRITICAL(&state_lock);
    bool enabled = s_link.gate && !s_link.reset && s_link.session.ready;
    packet.generation = s_link.generation;
    portEXIT_CRITICAL(&state_lock);
    if (!enabled)
    {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(packet.data, buffer, length);
    if (xQueueSend(s_link.tx_queue, &packet, 0) != pdTRUE)
    {
        record_packet_drop();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
static void release_rx_buffer(void *handle, void *buffer)
{
    (void)handle;
    packet_t *packet = (packet_t *)((uint8_t *)buffer - offsetof(packet_t, data));
    /* Pool owns the bytes, lwIP owns them only until this callback. */
    configASSERT(xQueueSend(s_link.free_rx_buffers, &packet, 0) == pdTRUE);
}
static esp_err_t attach_netif_driver(esp_netif_t *netif, void *args)
{
    (void)args;
    s_link.base.netif = netif;
    esp_netif_driver_ifconfig_t cfg = {
        .handle = &s_link,
        .transmit = enqueue_tx_packet,
        .driver_free_rx_buffer = release_rx_buffer,
    };
    return esp_netif_set_driver_config(netif, &cfg);
}
static void rotate_local_session(void)
{
    uint64_t id = create_session_id();
    portENTER_CRITICAL(&state_lock);
    gw_session_init(&s_link.session, id);
    s_link.gate = false;
    s_link.reset = false;
    s_link.remote_valid = false;
    ++s_link.generation;
    s_link.tx_sequence = 0;
    portEXIT_CRITICAL(&state_lock);
    xQueueReset(s_link.tx_queue);
}
static void service_session_timeout(void)
{
    uint64_t now = now_ms();
    portENTER_CRITICAL(&state_lock);
    bool reset = s_link.reset || gw_session_expired(&s_link.session, now) ||
                 s_link.tx_sequence == UINT32_MAX;
    portEXIT_CRITICAL(&state_lock);
    if (reset)
    {
        rotate_local_session();
    }
}
/* Snapshot one pairing before queueing DMA. Completion must still belong to
 * that pairing before its data may enter the network stack. */
static uint32_t prepare_tx_frame(uint64_t *local_id, uint64_t *peer_id)
{
    packet_t packet;
    uint8_t status_bytes[GW_STATUS_SIZE];
    gw_status_t status;
    gw_frame_t frame = {0};
    static uint64_t last_status;
    uint64_t now = now_ms();
    portENTER_CRITICAL(&state_lock);
    frame.sender = s_link.session.local;
    frame.peer = s_link.session.peer;
    frame.sequence = ++s_link.tx_sequence;
    bool ready = s_link.session.ready, enabled = s_link.gate && !s_link.reset;
    uint32_t generation = s_link.generation;
    status = s_link.status;
    status.received = s_link.received;
    status.rejected = s_link.rejected;
    status.dropped = s_link.dropped;
    if (!enabled)
    {
        status.flags &= ~GW_FORWARDING;
    }
    portEXIT_CRITICAL(&state_lock);
    frame.type = ready ? GW_IDLE : GW_HELLO;
#ifndef CONFIG_GW_SPI_CONTROLLER
    if (ready && now - last_status >= 250)
    {
        gw_status_encode(status_bytes, &status);
        frame.type = GW_STATUS;
        frame.payload = status_bytes;
        frame.length = GW_STATUS_SIZE;
        last_status = now;
    }
#else
    (void)status;
    (void)status_bytes;
    (void)last_status;
    (void)now;
#endif
    if (ready && enabled && frame.type == GW_IDLE)
    {
        while (xQueueReceive(s_link.tx_queue, &packet, 0) == pdTRUE)
        {
            if (packet.generation != generation)
            {
                record_packet_drop();
                continue;
            }
            frame.type = GW_DATA;
            frame.payload = packet.data;
            frame.length = packet.length;
            break;
        }
    }
    if (uxQueueMessagesWaiting(s_link.tx_queue))
    {
        frame.flags = GW_MORE;
    }
    configASSERT(gw_encode(s_link.tx_dma, &frame));
    *local_id = frame.sender;
    *peer_id = frame.peer;
    return generation;
}
/* Returns whether the controller should poll again without its idle delay. */
static bool process_rx_frame(size_t length, uint32_t sent_generation, uint64_t sent_local,
                             uint64_t sent_peer)
{
    gw_frame_t frame;
    if (!gw_decode(s_link.rx_dma, length, &frame))
    {
        portENTER_CRITICAL(&state_lock);
        ++s_link.rejected;
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    uint64_t now = now_ms();
    portENTER_CRITICAL(&state_lock);
    bool stale = s_link.reset || s_link.session.local != sent_local;
    gw_accept_t result = stale ? GW_REJECT : gw_session_accept(&s_link.session, &frame, now);
    if (result == GW_ACCEPT)
    {
        ++s_link.received;
    }
    else
    {
        ++s_link.rejected;
    }
    bool data = result == GW_ACCEPT && frame.type == GW_DATA && s_link.gate &&
                sent_generation == s_link.generation && sent_peer == frame.sender;
    if (result == GW_ACCEPT && frame.type == GW_STATUS)
    {
        gw_status_decode(&s_link.remote, frame.payload);
        s_link.remote_valid = true;
    }
    portEXIT_CRITICAL(&state_lock);
    if (result == GW_PEER_CHANGED)
    {
        gateway_link_request_reset();
        return false;
    }
    if (result != GW_ACCEPT)
    {
        return false;
    }
    if (frame.type == GW_DATA)
    {
        packet_t *packet;
        if (!data || xQueueReceive(s_link.free_rx_buffers, &packet, 0) != pdTRUE)
        {
            record_packet_drop();
        }
        else
        {
            memcpy(packet->data, frame.payload, frame.length);
            /* ethernetif_input consumes buffer ownership on success AND failure. */
            if (esp_netif_receive(s_link.netif, packet->data, frame.length, NULL) != ESP_OK)
            {
                record_packet_drop();
            }
        }
    }
    return frame.type == GW_DATA || (frame.flags & GW_MORE);
}
#ifndef CONFIG_GW_SPI_CONTROLLER
static void IRAM_ATTR on_spi_ready(spi_slave_transaction_t *t)
{
    (void)t;
    gpio_set_level(HS_GPIO, 1);
}
static void IRAM_ATTR on_spi_complete(spi_slave_transaction_t *t)
{
    (void)t;
    gpio_set_level(HS_GPIO, 0);
}
#endif
/* Only this task touches the SPI driver and DMA descriptors. */
static void spi_transport_task(void *arg)
{
    (void)arg;
    spi_bus_config_t bus = {.mosi_io_num = 11,
                            .miso_io_num = 13,
                            .sclk_io_num = 12,
                            .quadwp_io_num = -1,
                            .quadhd_io_num = -1,
                            .max_transfer_sz = DMA_SIZE};
    gpio_config_t hs = {.pin_bit_mask = 1ULL << HS_GPIO};
#ifdef CONFIG_GW_SPI_CONTROLLER
    hs.mode = GPIO_MODE_INPUT;
    hs.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&hs));
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t config = {
        .mode = 0, .clock_speed_hz = CONFIG_GW_SPI_CLOCK_HZ, .spics_io_num = 10, .queue_size = 1};
    spi_device_handle_t device;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &config, &device));
#else
    hs.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&hs));
    gpio_set_level(HS_GPIO, 0);
    ESP_ERROR_CHECK(gpio_set_pull_mode(10, GPIO_PULLUP_ONLY));
    spi_slave_interface_config_t config = {.mode = 0,
                                           .spics_io_num = 10,
                                           .queue_size = 1,
                                           .post_setup_cb = on_spi_ready,
                                           .post_trans_cb = on_spi_complete};
    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &bus, &config, SPI_DMA_CH_AUTO));
#endif
    ESP_LOGI(TAG, "SPI ready: SCLK=12 MOSI=11 MISO=13 CS=10 HS=4; protocol 0.2.0");
    for (;;)
    {
        service_session_timeout();
#ifdef CONFIG_GW_SPI_CONTROLLER
        if (!gpio_get_level(HS_GPIO))
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
#endif
        uint64_t local, peer;
        uint32_t generation = prepare_tx_frame(&local, &peer);
        memset(s_link.rx_dma, 0, DMA_SIZE);
        bool more = false;
#ifdef CONFIG_GW_SPI_CONTROLLER
        spi_transaction_t transaction = {
            .length = GW_FRAME_SIZE * 8, .tx_buffer = s_link.tx_dma, .rx_buffer = s_link.rx_dma};
        esp_err_t err = spi_device_transmit(device, &transaction);
        if (err == ESP_OK)
        {
            more = process_rx_frame(GW_FRAME_SIZE, generation, local, peer);
        }
        else
        {
            gateway_link_request_reset();
        }
        if (!more && !uxQueueMessagesWaiting(s_link.tx_queue))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
#else
        spi_slave_transaction_t transaction = {
            .length = DMA_SIZE * 8, .tx_buffer = s_link.tx_dma, .rx_buffer = s_link.rx_dma};
        ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &transaction, portMAX_DELAY));
        spi_slave_transaction_t *done;
        /* A timeout never releases DMA ownership. Keep the same buffers queued. */
        esp_err_t result;
        while ((result = spi_slave_get_trans_result(SPI2_HOST, &done, pdMS_TO_TICKS(20))) ==
               ESP_ERR_TIMEOUT)
        {
            service_session_timeout();
        }
        ESP_ERROR_CHECK(result);
        more = process_rx_frame(transaction.trans_len == GW_FRAME_SIZE * 8 ? GW_FRAME_SIZE : 0,
                                generation, local, peer);
        (void)more;
#endif
    }
}
esp_err_t gateway_link_init(void)
{
    if (s_link.netif)
    {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ip4_addr_t b, a, mask;
    if (esp_netif_str_to_ip4(CONFIG_GW_BRIDGE_IP, &b) != ESP_OK ||
        esp_netif_str_to_ip4(CONFIG_GW_HOST_IP, &a) != ESP_OK ||
        esp_netif_str_to_ip4(CONFIG_GW_LINK_MASK, &mask) != ESP_OK)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!gw_link_addresses_valid(ntohl(b.addr), ntohl(a.addr), ntohl(mask.addr)))
    {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_GW_SPI_CONTROLLER
    s_link.ip = (esp_netif_ip_info_t){.ip = a, .netmask = mask, .gw = b};
    s_link.peer_ip = b.addr;
#else
    s_link.ip = (esp_netif_ip_info_t){.ip = b, .netmask = mask};
    s_link.peer_ip = a.addr;
#endif
    s_link.tx_queue = xQueueCreate(QUEUE_DEPTH, sizeof(packet_t));
    s_link.free_rx_buffers = xQueueCreate(QUEUE_DEPTH, sizeof(packet_t *));
    s_link.tx_dma = heap_caps_calloc(1, DMA_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_link.rx_dma = heap_caps_calloc(1, DMA_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_link.tx_queue || !s_link.free_rx_buffers || !s_link.tx_dma || !s_link.rx_dma)
    {
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < QUEUE_DEPTH; ++i)
    {
        packet_t *packet = &s_link.rx_pool[i];
        xQueueSend(s_link.free_rx_buffers, &packet, 0);
    }
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.flags = 0;
    base.get_ip_event = 0;
    base.lost_ip_event = 0;
    base.if_key = "SPI_LINK";
    base.if_desc = "spi";
    base.route_prio = 10;
    base.ip_info = &s_link.ip;
    ESP_ERROR_CHECK(esp_read_mac(base.mac, ESP_MAC_WIFI_STA));
    base.mac[0] = (base.mac[0] | 2) & 0xfe;
    base.mac[5] ^= 0x80;
    esp_netif_config_t cfg = {.base = &base, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    s_link.netif = esp_netif_new(&cfg);
    if (!s_link.netif)
    {
        return ESP_ERR_NO_MEM;
    }
    s_link.base.post_attach = attach_netif_driver;
    ESP_ERROR_CHECK(esp_netif_attach(s_link.netif, &s_link));
    esp_netif_action_start(s_link.netif, NULL, 0, NULL);
    rotate_local_session();
    if (xTaskCreate(spi_transport_task, "spi_link", 6144, NULL, 8, NULL) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
