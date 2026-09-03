/**
 * @file ble_audio_vcp.c
 * @brief Volume Control Profile (VCP) Volume Renderer implementation.
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"

#include "esp_ble_audio_vcp_api.h"
#include "ble_audio_vcp.h"
#include "ble_audio_i2s.h"

static const char *TAG = "BLE_AUDIO_VCP";

static uint8_t s_volume = 200;
static bool s_muted = false;
static ble_audio_vcp_vol_cb_t s_vol_cb = NULL;

static void vol_rend_state_cb(struct bt_conn *conn, int err, uint8_t volume, uint8_t mute)
{
    if (err != 0) {
        ESP_LOGW(TAG, "VCP state callback error: %d", err);
        return;
    }

    s_volume = volume;
    s_muted = (mute == BT_VCP_STATE_MUTED);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, ">>> VCP VOLUME UPDATED: %u / 255 (Muted: %s) <<<", s_volume, s_muted ? "YES" : "NO");
    ESP_LOGI(TAG, "==================================================");

    ble_audio_i2s_set_mute(s_muted);

    if (s_vol_cb != NULL) {
        s_vol_cb(s_volume, s_muted);
    }
}

static void vol_rend_flags_cb(struct bt_conn *conn, int err, uint8_t flags)
{
    ESP_LOGI(TAG, ">>> VCP Flags Updated: err=%d, flags=0x%02x <<<", err, flags);
}

static struct bt_vcp_vol_rend_cb s_vcp_cb = {
    .state = vol_rend_state_cb,
    .flags = vol_rend_flags_cb,
};

esp_err_t ble_audio_vcp_init(void)
{
    ESP_LOGI(TAG, "Registering VCP Volume Control Service (VCS)...");

    struct bt_vcp_vol_rend_register_param param = {0};
    param.step = 1;
    param.mute = BT_VCP_STATE_UNMUTED;
    param.volume = s_volume;
    param.cb = &s_vcp_cb;

    esp_err_t err = esp_ble_audio_vcp_vol_rend_register(&param);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register VCP Volume Renderer: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "VCP Volume Renderer registered successfully (Initial Vol: %u/255)", s_volume);
    return ESP_OK;
}

uint8_t ble_audio_vcp_get_volume(void)
{
    return s_volume;
}

bool ble_audio_vcp_is_muted(void)
{
    return s_muted;
}

void ble_audio_vcp_set_vol_cb(ble_audio_vcp_vol_cb_t cb)
{
    s_vol_cb = cb;
}

void ble_audio_vcp_apply_volume(int16_t *samples, size_t count)
{
    if (samples == NULL || count == 0) {
        return;
    }

    if (s_muted) {
        memset(samples, 0, count * sizeof(int16_t));
        return;
    }

    if (s_volume == 255) {
        return; // Full dynamic range (0 dBFS)
    }

    uint32_t vol = s_volume;
    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(((int32_t)samples[i] * vol) / 255);
    }
}

