/**
 * @file ble_audio_sink.cpp
 * @brief Bluetooth LE Audio Sink Component Implementation Stub (Phase 1 Baseline).
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ble_audio_sink.h"
#include "ble_audio_gap.h"

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
    ESP_LOGI(TAG, "Initializing BLE Audio Sink: device_name='%s', I2S(BCLK=%d, WS=%d, DOUT=%d)",
             s_config.device_name ? s_config.device_name : "default",
             s_config.bclk_pin, s_config.ws_pin, s_config.dout_pin);

    /* Initialize BLE GAP, Security (Just Works) and Auto-reconnect */
    esp_err_t ret = ble_audio_gap_init(s_config.device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE GAP subsystem: %d", ret);
        return ret;
    }

    return ESP_OK;
}

esp_err_t ble_audio_sink_start(void)
{
    ESP_LOGI(TAG, "Starting BLE Audio Sink subsystem (Phase 2 GAP & Extended Advertising)...");
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
}

void ble_audio_sink_set_metadata_callback(ble_audio_metadata_cb_t cb)
{
    s_metadata_cb = cb;
}

void ble_audio_sink_set_play_pos_callback(ble_audio_play_pos_cb_t cb)
{
    s_play_pos_cb = cb;
}

void ble_audio_sink_play(void)
{
    ESP_LOGD(TAG, "MCP command -> PLAY");
}

void ble_audio_sink_pause(void)
{
    ESP_LOGD(TAG, "MCP command -> PAUSE");
}

void ble_audio_sink_stop(void)
{
    ESP_LOGD(TAG, "MCP command -> STOP");
}

void ble_audio_sink_next(void)
{
    ESP_LOGD(TAG, "MCP command -> NEXT_TRACK");
}

void ble_audio_sink_previous(void)
{
    ESP_LOGD(TAG, "MCP command -> PREV_TRACK");
}

