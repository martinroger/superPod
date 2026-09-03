/**
 * @file ble_audio_sink.cpp
 * @brief Bluetooth LE Audio Sink Facade Implementation (Phase 4).
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ble_audio_sink.h"
#include "ble_audio_gap.h"
#include "ble_audio_i2s.h"
#include "ble_audio_bap.h"
#include "ble_audio_vcp.h"
#include "ble_audio_mcc.h"

static const char *TAG = "BLE_AUDIO_SINK";

static ble_audio_sink_config_t s_config;
static ble_audio_conn_state_cb_t s_conn_cb = nullptr;
static void *s_conn_user_data = nullptr;
static ble_audio_state_cb_t s_audio_cb = nullptr;
static void *s_audio_user_data = nullptr;
static ble_audio_metadata_cb_t s_metadata_cb = nullptr;
static ble_audio_play_pos_cb_t s_play_pos_cb = nullptr;

esp_err_t ble_audio_sink_init(const ble_audio_sink_config_t *config)
{
    if (config == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    ESP_LOGI(TAG, "Initializing BLE Audio Sink: device_name='%s', I2S(BCLK=%d, WS=%d, DOUT=%d, MutePin=%d)",
             s_config.device_name ? s_config.device_name : "default",
             s_config.bclk_pin, s_config.ws_pin, s_config.dout_pin, s_config.mute_pin);

    /* Step 1: Initialize native I2S DMA driver and mute pin (REQ-AUD-2, REQ-FLOW) */
    ble_audio_i2s_config_t i2s_cfg = {
        .bclk_pin = s_config.bclk_pin,
        .ws_pin = s_config.ws_pin,
        .dout_pin = s_config.dout_pin,
        .mclk_pin = s_config.mclk_pin,
        .mute_pin = s_config.mute_pin,
        .default_sample_rate = s_config.default_sample_rate > 0 ? s_config.default_sample_rate : 48000,
    };
    esp_err_t ret = ble_audio_i2s_init(&i2s_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize native I2S driver: %d", ret);
        return ret;
    }

    /* Step 2: Initialize BLE GAP, Security (Just Works) and Auto-reconnect (REQ-SEC, REQ-CONN) */
    ret = ble_audio_gap_init(s_config.device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE GAP subsystem: %d", ret);
        return ret;
    }

    /* Step 3: Initialize BAP Unicast Sink, PACS & ASCS endpoints (initializes esp_ble_audio_common_init) */
    ret = ble_audio_bap_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BAP Unicast Sink: %d", ret);
        return ret;
    }

    /* Step 4: Initialize VCP Volume Renderer (REQ-VOL) */
    ret = ble_audio_vcp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize VCP Volume Renderer: %d", ret);
        return ret;
    }

    /* Step 5: Initialize Media Control Client (REQ-CTRL, REQ-META) */
    ret = ble_audio_mcc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Media Control Client: %d", ret);
        return ret;
    }

    return ESP_OK;
}

esp_err_t ble_audio_sink_start(void)
{
    ESP_LOGI(TAG, "Starting BLE Audio Sink subsystem (Phase 4 BAP, VCP, MCP & Extended Advertising)...");
    esp_err_t ret = ble_audio_bap_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BAP subsystem: %d", ret);
        return ret;
    }

    return ble_audio_gap_start_advertising();
}

void ble_audio_sink_set_connection_state_callback(ble_audio_conn_state_cb_t cb, void *user_data)
{
    s_conn_cb = cb;
    s_conn_user_data = user_data;
    ble_audio_gap_set_conn_cb(cb, user_data);
}

void ble_audio_sink_set_audio_state_callback(ble_audio_state_cb_t cb, void *user_data)
{
    s_audio_cb = cb;
    s_audio_user_data = user_data;
    ble_audio_bap_set_audio_state_cb(cb, user_data);
    ble_audio_mcc_set_state_cb(cb, user_data);
}

void ble_audio_sink_set_metadata_callback(ble_audio_metadata_cb_t cb)
{
    s_metadata_cb = cb;
    ble_audio_bap_set_metadata_cb(cb);
    ble_audio_mcc_set_metadata_cb(cb);
}

void ble_audio_sink_set_play_pos_callback(ble_audio_play_pos_cb_t cb)
{
    s_play_pos_cb = cb;
    ble_audio_bap_set_play_pos_cb(cb);
    ble_audio_mcc_set_play_pos_cb(cb);
}

void ble_audio_sink_play(void)
{
    ESP_LOGI(TAG, "Transport command -> PLAY");
    ble_audio_mcc_play();
}

void ble_audio_sink_pause(void)
{
    ESP_LOGI(TAG, "Transport command -> PAUSE");
    ble_audio_mcc_pause();
}

void ble_audio_sink_stop(void)
{
    ESP_LOGI(TAG, "Transport command -> STOP");
    ble_audio_mcc_stop();
}

void ble_audio_sink_next(void)
{
    ESP_LOGI(TAG, "Transport command -> NEXT_TRACK");
    ble_audio_mcc_next();
}

void ble_audio_sink_previous(void)
{
    ESP_LOGI(TAG, "Transport command -> PREV_TRACK");
    ble_audio_mcc_previous();
}

uint8_t ble_audio_sink_get_volume(void)
{
    return ble_audio_vcp_get_volume();
}

bool ble_audio_sink_is_muted(void)
{
    return ble_audio_vcp_is_muted();
}

void ble_audio_sink_set_volume_callback(void (*cb)(uint8_t volume, bool muted))
{
    ble_audio_vcp_set_vol_cb(cb);
}
