/**
 * @file bt_a2dp_sink.cpp
 * @brief Implementation of Native ESP-IDF Bluetooth Classic A2DP Sink & AVRCP Controller.
 */

#include "bt_a2dp_sink.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "nvs_flash.h"

static const char *TAG = "BT_A2DP_SINK";

static bt_a2dp_connection_state_cb_t s_conn_cb = NULL;
static void *s_conn_user_data = NULL;

static bt_a2dp_audio_state_cb_t s_audio_cb = NULL;
static void *s_audio_user_data = NULL;

static bt_avrc_metadata_cb_t s_meta_cb = NULL;
static bt_avrc_play_pos_cb_t s_pos_cb = NULL;
static uint32_t s_pos_interval_s = 1;

static bool s_initialized = false;
static char s_device_name[32] = "superPod-A2DP";

// Forward declarations
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

esp_err_t bt_a2dp_sink_init(const bt_a2dp_sink_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "bt_a2dp_sink already initialized");
        return ESP_OK;
    }

    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->device_name != NULL) {
        strncpy(s_device_name, config->device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }

    // 1. Initialize NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Initialize I2S Audio Driver
    ret = i2s_audio_init(&config->i2s_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S Audio init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Release Bluetooth Controller Memory (BLE mode if unused)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // 4. Initialize BT Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 5. Initialize Bluedroid Stack
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 6. Set GAP Device Name
    esp_bt_gap_set_device_name(s_device_name);

    // 7. Register A2DP Sink Callbacks
    ret = esp_a2d_register_callback(bt_app_a2d_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_a2d_register_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_a2d_sink_register_data_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_a2d_sink_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_a2d_sink_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 8. Register AVRCP Controller Callbacks
    ret = esp_avrc_ct_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_avrc_ct_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_avrc_ct_register_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Bluetooth A2DP Sink initialized successfully (%s)", s_device_name);
    return ESP_OK;
}

esp_err_t bt_a2dp_sink_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Enable discoverability and connectability
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    ESP_LOGI(TAG, "Bluetooth GAP scan mode set: Connectable & Discoverable");
    return ESP_OK;
}

void bt_a2dp_sink_set_connection_state_cb(bt_a2dp_connection_state_cb_t cb, void *user_data)
{
    s_conn_cb = cb;
    s_conn_user_data = user_data;
}

void bt_a2dp_sink_set_audio_state_cb(bt_a2dp_audio_state_cb_t cb, void *user_data)
{
    s_audio_cb = cb;
    s_audio_user_data = user_data;
}

void bt_a2dp_sink_set_metadata_cb(bt_avrc_metadata_cb_t cb)
{
    s_meta_cb = cb;
}

void bt_a2dp_sink_set_play_pos_cb(bt_avrc_play_pos_cb_t cb, uint32_t interval_s)
{
    s_pos_cb = cb;
    if (interval_s > 0) {
        s_pos_interval_s = interval_s;
    }
}

static esp_err_t send_avrc_passthrough_cmd(uint8_t key_code)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(0, key_code, ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret == ESP_OK) {
        esp_avrc_ct_send_passthrough_cmd(0, key_code, ESP_AVRC_PT_CMD_STATE_RELEASED);
    } else {
        ESP_LOGE(TAG, "Failed to send AVRCP key %d: %s", key_code, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bt_a2dp_sink_play(void)
{
    return send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PLAY);
}

esp_err_t bt_a2dp_sink_pause(void)
{
    return send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE);
}

esp_err_t bt_a2dp_sink_stop(void)
{
    return send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_STOP);
}

esp_err_t bt_a2dp_sink_next(void)
{
    return send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_FORWARD);
}

esp_err_t bt_a2dp_sink_previous(void)
{
    return send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_BACKWARD);
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG, "A2DP Connection State Event: %d", param->conn_stat.state);
        if (s_conn_cb != NULL) {
            s_conn_cb(param->conn_stat.state, s_conn_user_data);
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "A2DP Audio State Event: %d", param->audio_stat.state);
        if (s_audio_cb != NULL) {
            s_audio_cb(param->audio_stat.state, s_audio_user_data);
        }
        break;

    case ESP_A2D_AUDIO_CFG_EVT: {
        ESP_LOGI(TAG, "A2DP Audio CFG Event, codec type: %d", param->audio_cfg.mcc.type);
        if (param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
            uint32_t sample_rate = 44100;
            uint8_t sf = param->audio_cfg.mcc.cie.sbc_info.samp_freq;
            if (sf & ESP_A2D_SBC_CIE_SF_44K) {
                sample_rate = 44100;
            } else if (sf & ESP_A2D_SBC_CIE_SF_48K) {
                sample_rate = 48000;
            } else if (sf & ESP_A2D_SBC_CIE_SF_32K) {
                sample_rate = 32000;
            } else if (sf & ESP_A2D_SBC_CIE_SF_16K) {
                sample_rate = 16000;
            }
            i2s_audio_set_sample_rate(sample_rate);
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled A2DP event: %d", event);
        break;
    }
}

static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    i2s_audio_write(data, len);
}

static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
        ESP_LOGI(TAG, "AVRCP CT Connection State: %d", param->conn_stat.connected);
        if (param->conn_stat.connected) {
            // Request track metadata attributes
            uint8_t attr_mask = ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                                ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME;
            esp_avrc_ct_send_metadata_cmd(0, attr_mask);

            // Register play position notification if callback set
            if (s_pos_cb != NULL) {
                esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_PLAY_POS_CHANGED, s_pos_interval_s);
            }
        }
        break;
    }

    case ESP_AVRC_CT_METADATA_RSP_EVT: {
        ESP_LOGI(TAG, "AVRCP Metadata Rsp attr: 0x%x", param->meta_rsp.attr_id);
        if (s_meta_cb != NULL && param->meta_rsp.attr_text != NULL) {
            s_meta_cb(param->meta_rsp.attr_id, param->meta_rsp.attr_text);
        }
        break;
    }

    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
        if (param->change_ntf.event_id == ESP_AVRC_RN_PLAY_POS_CHANGED) {
            uint32_t pos_ms = param->change_ntf.event_parameter.play_pos;
            if (s_pos_cb != NULL) {
                s_pos_cb(pos_ms);
                // Re-register notification for continuous play position reporting
                esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_PLAY_POS_CHANGED, s_pos_interval_s);
            }
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled AVRCP CT event: %d", event);
        break;
    }
}
