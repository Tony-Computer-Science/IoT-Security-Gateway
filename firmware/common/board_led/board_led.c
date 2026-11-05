#include "board_led.h"
#include "sdkconfig.h"
#ifdef CONFIG_GW_BOARD_RGB_LED
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#endif

esp_err_t board_led_show_role(bool station)
{
#ifdef CONFIG_GW_BOARD_RGB_LED
    /* One GRB pixel, MSB first, between 300 us reset/latch intervals. Pre-encode all
     * symbols so this one-shot update needs no custom ISR or extra task. */
    uint8_t grb[3] = {0, station ? 0 : 96, station ? 96 : 0};
    rmt_symbol_word_t symbols[26];
    symbols[0] = (rmt_symbol_word_t){.duration0 = 1500, .duration1 = 1500};
    for (unsigned bit = 0; bit < 24; ++bit)
    {
        bool one = grb[bit / 8] & (0x80u >> (bit % 8));
        symbols[bit + 1] = (rmt_symbol_word_t){
            .level0 = 1, .duration0 = one ? 9 : 3, .level1 = 0, .duration1 = one ? 3 : 9};
    }
    symbols[25] = symbols[0];
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;
    bool enabled = false;
    rmt_tx_channel_config_t config = {.gpio_num = CONFIG_GW_BOARD_RGB_GPIO,
                                      .clk_src = RMT_CLK_SRC_DEFAULT,
                                      .resolution_hz = 10000000,
                                      .mem_block_symbols = 64,
                                      .trans_queue_depth = 1};
    esp_err_t error = rmt_new_tx_channel(&config, &channel);
    if (error != ESP_OK)
    {
        return error;
    }
    rmt_copy_encoder_config_t copy_config = {};
    error = rmt_new_copy_encoder(&copy_config, &encoder);
    if (error == ESP_OK)
    {
        error = rmt_enable(channel);
        enabled = error == ESP_OK;
    }
    if (error == ESP_OK)
    {
        rmt_transmit_config_t transmit = {0};
        error = rmt_transmit(channel, encoder, symbols, sizeof(symbols), &transmit);
        if (error == ESP_OK)
        {
            /* Keep stack-backed symbols alive until transmission finishes. */
            error = rmt_tx_wait_all_done(channel, -1);
        }
    }
    if (enabled)
    {
        rmt_disable(channel);
    }
    if (encoder)
    {
        rmt_del_encoder(encoder);
    }
    rmt_del_channel(channel);
    /* The pixel retains its color; keep its data input low between updates. */
    gpio_set_level(CONFIG_GW_BOARD_RGB_GPIO, 0);
    gpio_set_direction(CONFIG_GW_BOARD_RGB_GPIO, GPIO_MODE_OUTPUT);
    if (error == ESP_OK)
    {
        ESP_LOGI("board_led", "%s identity: %s RGB LED on GPIO%d", station ? "Station" : "Bridge",
                 station ? "blue" : "red", CONFIG_GW_BOARD_RGB_GPIO);
    }
    return error;
#else
    (void)station;
    return ESP_OK;
#endif
}
