/**
 * @file ble_audio_i2s.cpp
 * @brief Native I2S DMA Audio Driver & Flow Manager Implementation (REQ-AUD-2, REQ-FLOW).
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_audio_i2s.h"

static const char *TAG = "BLE_AUDIO_I2S";

static i2s_chan_handle_t s_tx_handle = nullptr;
static ble_audio_i2s_config_t s_config;
static bool s_is_running = false;
static uint32_t s_current_sample_rate = 48000;

#define MONO_TO_STEREO_MAX_SAMPLES 1024
static int16_t s_stereo_scratch[MONO_TO_STEREO_MAX_SAMPLES * 2];

esp_err_t ble_audio_i2s_init(const ble_audio_i2s_config_t *config)
{
    if (config == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    s_current_sample_rate = (s_config.default_sample_rate > 0) ? s_config.default_sample_rate : 48000;

    ESP_LOGI(TAG, "Initializing native I2S standard driver: BCLK=%d, WS=%d, DOUT=%d, Rate=%lu Hz, MutePin=%d",
             s_config.bclk_pin, s_config.ws_pin, s_config.dout_pin,
             (unsigned long)s_current_sample_rate, s_config.mute_pin);

    /* Step 1: Configure hardware mute pin if present (REQ-FLOW-4) */
    if (s_config.mute_pin >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << s_config.mute_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        ble_audio_i2s_set_mute(true); // Start in muted state
    }

    /* Step 2: Allocate I2S channel with DMA ring buffer (REQ-FLOW-3) */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;
    chan_cfg.auto_clear = true; // Clear DMA on underflow to suppress noise

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_handle, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate I2S channel: %d", ret);
        return ret;
    }

    /* Step 3: Initialize standard Philips I2S mode (16-bit stereo for UDA1334A DAC) */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_current_sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (s_config.mclk_pin >= 0) ? (gpio_num_t)s_config.mclk_pin : I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)s_config.bclk_pin,
            .ws = (gpio_num_t)s_config.ws_pin,
            .dout = (gpio_num_t)s_config.dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize standard mode: %d", ret);
        i2s_del_channel(s_tx_handle);
        s_tx_handle = nullptr;
        return ret;
    }

    ESP_LOGI(TAG, "Native I2S driver initialized successfully");
    return ESP_OK;
}

esp_err_t ble_audio_i2s_start(void)
{
    if (s_tx_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_is_running) {
        esp_err_t ret = i2s_channel_enable(s_tx_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable I2S channel: %d", ret);
            return ret;
        }
        s_is_running = true;
    }

    // Unmute DAC output (REQ-AUD-2, REQ-FLOW-4)
    ble_audio_i2s_set_mute(false);
    ESP_LOGI(TAG, "I2S DMA channel enabled and unmuted");
    return ESP_OK;
}

esp_err_t ble_audio_i2s_stop(void)
{
    if (s_tx_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Mute hardware immediately to prevent pops (REQ-FLOW-4)
    ble_audio_i2s_set_mute(true);

    if (s_is_running) {
        // Output a brief zero-fill frame before disabling
        int16_t zero_buf[64] = {0};
        size_t written = 0;
        i2s_channel_write(s_tx_handle, zero_buf, sizeof(zero_buf), &written, pdMS_TO_TICKS(10));

        esp_err_t ret = i2s_channel_disable(s_tx_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to disable I2S channel: %d", ret);
        }
        s_is_running = false;
    }

    ESP_LOGI(TAG, "I2S DMA channel stopped and muted");
    return ESP_OK;
}

esp_err_t ble_audio_i2s_set_sample_rate(uint32_t sample_rate_hz)
{
    if (s_tx_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sample_rate_hz == 0 || sample_rate_hz == s_current_sample_rate) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Reconfiguring I2S sample rate on-the-fly: %lu Hz -> %lu Hz (REQ-FLOW-1)",
             (unsigned long)s_current_sample_rate, (unsigned long)sample_rate_hz);

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_hz);
    esp_err_t ret = i2s_channel_reconfig_std_clock(s_tx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure I2S clock: %d", ret);
        return ret;
    }

    s_current_sample_rate = sample_rate_hz;
    return ESP_OK;
}

esp_err_t ble_audio_i2s_write(const void *pcm_data, size_t pcm_bytes, uint8_t channels)
{
    if (s_tx_handle == nullptr || pcm_data == nullptr || pcm_bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_is_running) {
        ble_audio_i2s_start();
    }

    size_t bytes_written = 0;

    /* REQ-FLOW-2: Automatic Mono-to-Stereo Duplication */
    if (channels == 1) {
        const int16_t *mono_samples = (const int16_t *)pcm_data;
        size_t sample_count = pcm_bytes / sizeof(int16_t);

        while (sample_count > 0) {
            size_t chunk = (sample_count > MONO_TO_STEREO_MAX_SAMPLES) ? MONO_TO_STEREO_MAX_SAMPLES : sample_count;
            for (size_t i = 0; i < chunk; i++) {
                s_stereo_scratch[i * 2] = mono_samples[i];     // Left
                s_stereo_scratch[i * 2 + 1] = mono_samples[i]; // Right
            }

            esp_err_t ret = i2s_channel_write(s_tx_handle, s_stereo_scratch, chunk * 2 * sizeof(int16_t),
                                              &bytes_written, pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                return ret;
            }

            mono_samples += chunk;
            sample_count -= chunk;
        }
        return ESP_OK;
    }

    /* Standard Stereo write with DMA backpressure (REQ-FLOW-3) */
    return i2s_channel_write(s_tx_handle, pcm_data, pcm_bytes, &bytes_written, pdMS_TO_TICKS(100));
}

void ble_audio_i2s_set_mute(bool muted)
{
    if (s_config.mute_pin >= 0) {
        // High = active (unmuted), Low = muted (or vice versa based on standard pull-down DACs)
        gpio_set_level((gpio_num_t)s_config.mute_pin, muted ? 0 : 1);
        ESP_LOGD(TAG, "Hardware mute pin %d set to %s", s_config.mute_pin, muted ? "MUTED" : "UNMUTED");
    }
}

void ble_audio_i2s_deinit(void)
{
    if (s_tx_handle != nullptr) {
        ble_audio_i2s_stop();
        i2s_del_channel(s_tx_handle);
        s_tx_handle = nullptr;
    }
}

