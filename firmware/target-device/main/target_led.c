#include "target_led.h"
#include "sdkconfig.h"
#include "driver/rmt_tx.h"

static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;

esp_err_t target_led_set(bool on)
{
    if (!channel || !encoder)
    {
        return ESP_ERR_INVALID_STATE;
    }
    /* Green=on, black=off. GPIO8 is a WS2812 data signal, not a plain LED. */
    uint8_t grb[3] = {on ? 96 : 0, 0, 0};
    rmt_symbol_word_t symbols[26];
    symbols[0] = (rmt_symbol_word_t){.duration0 = 1500, .duration1 = 1500};
    for (unsigned bit = 0; bit < 24; ++bit)
    {
        bool one = grb[bit / 8] & (0x80u >> (bit % 8));
        symbols[bit + 1] = (rmt_symbol_word_t){
            .level0 = 1, .duration0 = one ? 9 : 3, .level1 = 0, .duration1 = one ? 3 : 9};
    }
    symbols[25] = symbols[0];
    rmt_transmit_config_t transmit = {0};
    esp_err_t error = rmt_transmit(channel, encoder, symbols, sizeof(symbols), &transmit);
    if (error == ESP_OK)
    {
        error = rmt_tx_wait_all_done(channel, -1);
    }
    return error;
}

esp_err_t target_led_init(void)
{
    rmt_tx_channel_config_t config = {.gpio_num = CONFIG_TARGET_LED_GPIO,
                                      .clk_src = RMT_CLK_SRC_DEFAULT,
                                      .resolution_hz = 10000000,
                                      .mem_block_symbols = 64,
                                      .trans_queue_depth = 1};
    esp_err_t error = rmt_new_tx_channel(&config, &channel);
    if (error != ESP_OK)
    {
        return error;
    }
    rmt_copy_encoder_config_t copy = {};
    error = rmt_new_copy_encoder(&copy, &encoder);
    if (error != ESP_OK)
    {
        return error;
    }
    error = rmt_enable(channel);
    return error == ESP_OK ? target_led_set(false) : error;
}
