/**
 * @file i2s_audio.cpp
 * @brief Implementation of I2S audio driver using ESP-IDF esp_driver_i2s API.
 */

#include "i2s_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "I2S_AUDIO";
static i2s_chan_handle_t tx_chan = NULL;
static uint32_t current_sample_rate = 44100;

esp_err_t i2s_audio_init(const i2s_audio_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (tx_chan != NULL) {
        ESP_LOGW(TAG, "I2S TX channel already initialized");
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(config->port, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S TX channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->bclk_pin,
            .ws = config->ws_pin,
            .dout = config->dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
        return ret;
    }

    ret = i2s_channel_enable(tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
        return ret;
    }

    current_sample_rate = config->sample_rate;
    ESP_LOGI(TAG, "I2S TX initialized successfully (BCLK:%d WS:%d DOUT:%d Rate:%luHz)",
             config->bclk_pin, config->ws_pin, config->dout_pin, current_sample_rate);
    return ESP_OK;
}

size_t i2s_audio_write(const uint8_t *data, size_t size)
{
    if (tx_chan == NULL || data == NULL || size == 0) {
        return 0;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_chan, data, size, &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    }
    return bytes_written;
}

esp_err_t i2s_audio_set_sample_rate(uint32_t sample_rate)
{
    if (tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sample_rate == current_sample_rate) {
        return ESP_OK;
    }

    i2s_channel_disable(tx_chan);
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    esp_err_t ret = i2s_channel_reconfig_std_clock(tx_chan, &clk_cfg);
    i2s_channel_enable(tx_chan);

    if (ret == ESP_OK) {
        current_sample_rate = sample_rate;
        ESP_LOGI(TAG, "Sample rate updated to %lu Hz", sample_rate);
    } else {
        ESP_LOGE(TAG, "Failed to update sample rate: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t i2s_audio_deinit(void)
{
    if (tx_chan != NULL) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
    return ESP_OK;
}
