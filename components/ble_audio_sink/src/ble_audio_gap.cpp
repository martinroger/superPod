/**
 * @file ble_audio_gap.cpp
 * @brief BLE GAP, Security & Auto-Reconnect Implementation (REQ-SEC, REQ-CONN).
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_ble_audio_defs.h"

#include "ble_audio_gap.h"

static const char *TAG = "BLE_AUDIO_GAP";

#define NVS_NAMESPACE       "ble_gap"
#define NVS_KEY_LAST_BDA    "last_bda"
#define ADV_HANDLE          0
#define ADV_SID             0
#define ADV_TX_POWER        10
#define ADV_INTERVAL_MS     25  // 20-30ms fast connectable advertising interval for BAP
#define AUTO_RECONNECT_MS   10000

static SemaphoreHandle_t s_adv_sem = nullptr;
static esp_bt_status_t s_adv_op_status = ESP_BT_STATUS_SUCCESS;
static bool s_is_connected = false;
static uint8_t s_connected_bda[6] = {0};
static uint8_t s_last_bonded_bda[6] = {0};
static bool s_has_bonded_bda = false;
static char s_device_name[32] = "superPod-Audio";
static uint8_t s_ext_adv_data[64];
static size_t s_ext_adv_data_len = 0;

static ble_audio_conn_state_cb_t s_conn_cb = nullptr;
static void *s_conn_user_data = nullptr;
static esp_timer_handle_t s_auto_reconnect_timer = nullptr;

#define WAIT_ADV_OP(_call) do { \
    esp_err_t _err = (_call); \
    if (_err != ESP_OK) { \
        ESP_LOGE(TAG, "API call failed: %d", _err); \
        return _err; \
    } \
    if (xSemaphoreTake(s_adv_sem, pdMS_TO_TICKS(3000)) != pdTRUE) { \
        ESP_LOGE(TAG, "Timeout waiting for adv operation"); \
        return ESP_ERR_TIMEOUT; \
    } \
    if (s_adv_op_status != ESP_BT_STATUS_SUCCESS) { \
        ESP_LOGE(TAG, "Adv operation returned status: %d", s_adv_op_status); \
        return ESP_FAIL; \
    } \
} while(0)

static esp_ble_gap_ext_adv_params_t s_ext_adv_params = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE,
    .interval_min = ESP_BLE_GAP_ADV_ITVL_MS(ADV_INTERVAL_MS),
    .interval_max = ESP_BLE_GAP_ADV_ITVL_MS(ADV_INTERVAL_MS),
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0},
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = ADV_TX_POWER,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_2M,
    .sid = ADV_SID,
    .scan_req_notif = false,
};

static esp_ble_gap_ext_adv_t s_ext_adv_inst[1] = {
    [0] = { ADV_HANDLE, 0, 0 },
};

static const uint8_t s_ext_adv_stop_inst[1] = { ADV_HANDLE };

/**
 * @brief Saves last bonded peer BDA to NVS storage (REQ-SEC-2, REQ-CONN-1).
 */
static void save_last_bda_to_nvs(const uint8_t bda[6])
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_blob(nvs_handle, NVS_KEY_LAST_BDA, bda, 6);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        memcpy(s_last_bonded_bda, bda, 6);
        s_has_bonded_bda = true;
        ESP_LOGI(TAG, "Saved bonded peer BDA to NVS: %02x:%02x:%02x:%02x:%02x:%02x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS to save peer BDA: %d", err);
    }
}

/**
 * @brief Loads last bonded peer BDA from NVS storage (REQ-CONN-1).
 */
static void load_last_bda_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = 6;
        err = nvs_get_blob(nvs_handle, NVS_KEY_LAST_BDA, s_last_bonded_bda, &required_size);
        if (err == ESP_OK && required_size == 6) {
            s_has_bonded_bda = true;
            ESP_LOGI(TAG, "Loaded bonded peer BDA from NVS: %02x:%02x:%02x:%02x:%02x:%02x",
                     s_last_bonded_bda[0], s_last_bonded_bda[1], s_last_bonded_bda[2],
                     s_last_bonded_bda[3], s_last_bonded_bda[4], s_last_bonded_bda[5]);
        }
        nvs_close(nvs_handle);
    }
}

/**
 * @brief Constructs connectable Extended Advertising raw payload.
 */
static void build_ext_adv_payload(void)
{
    size_t idx = 0;
    size_t name_len = strlen(s_device_name);

    /* 1. Flags (General Discoverable + BR/EDR Not Supported) */
    s_ext_adv_data[idx++] = 0x02;
    s_ext_adv_data[idx++] = ESP_BLE_AD_TYPE_FLAG;
    s_ext_adv_data[idx++] = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;

    /* 2. Incomplete List of 16-bit Service UUIDs (ASCS: 0x184E) */
    s_ext_adv_data[idx++] = 0x03;
    s_ext_adv_data[idx++] = ESP_BLE_AD_TYPE_16SRV_PART;
    s_ext_adv_data[idx++] = (ESP_BLE_AUDIO_UUID_ASCS_VAL & 0xFF);
    s_ext_adv_data[idx++] = ((ESP_BLE_AUDIO_UUID_ASCS_VAL >> 8) & 0xFF);

    /* 3. Service Data 16-bit UUID (Targeted Unicast Announcement for Audio Contexts) */
    uint16_t sink_context = ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED |
                            ESP_BLE_AUDIO_CONTEXT_TYPE_CONVERSATIONAL |
                            ESP_BLE_AUDIO_CONTEXT_TYPE_MEDIA;
    uint16_t source_context = sink_context;

    s_ext_adv_data[idx++] = 0x09;
    s_ext_adv_data[idx++] = ESP_BLE_AD_TYPE_SERVICE_DATA;
    s_ext_adv_data[idx++] = (ESP_BLE_AUDIO_UUID_ASCS_VAL & 0xFF);
    s_ext_adv_data[idx++] = ((ESP_BLE_AUDIO_UUID_ASCS_VAL >> 8) & 0xFF);
    s_ext_adv_data[idx++] = ESP_BLE_AUDIO_UNICAST_ANNOUNCEMENT_TARGETED;
    s_ext_adv_data[idx++] = (sink_context & 0xFF);
    s_ext_adv_data[idx++] = ((sink_context >> 8) & 0xFF);
    s_ext_adv_data[idx++] = (source_context & 0xFF);
    s_ext_adv_data[idx++] = ((source_context >> 8) & 0xFF);
    s_ext_adv_data[idx++] = 0x00; /* Metadata length */

    /* 4. Complete Local Device Name */
    if (idx + 2 + name_len <= sizeof(s_ext_adv_data)) {
        s_ext_adv_data[idx++] = (uint8_t)(name_len + 1);
        s_ext_adv_data[idx++] = ESP_BLE_AD_TYPE_NAME_CMPL;
        memcpy(&s_ext_adv_data[idx], s_device_name, name_len);
        idx += name_len;
    }

    s_ext_adv_data_len = idx;
}

/**
 * @brief Auto-reconnection timer callback running every 10 seconds (REQ-CONN-1).
 */
static void auto_reconnect_timer_callback(void *arg)
{
    if (s_is_connected) {
        return;
    }

    if (s_has_bonded_bda) {
        ESP_LOGI(TAG, "Auto-reconnect interval (10s): waiting for bonded peer %02x:%02x:%02x:%02x:%02x:%02x",
                 s_last_bonded_bda[0], s_last_bonded_bda[1], s_last_bonded_bda[2],
                 s_last_bonded_bda[3], s_last_bonded_bda[4], s_last_bonded_bda[5]);
    } else {
        ESP_LOGD(TAG, "Auto-reconnect interval (10s): advertising connectable BLE Audio");
    }

    // Refresh connectable advertising
    esp_ble_gap_ext_adv_start(1, s_ext_adv_inst);
}

/**
 * @brief Bluedroid GAP event handler for advertising and Just-Works security.
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
        s_adv_op_status = param->ext_adv_set_params.status;
        xSemaphoreGive(s_adv_sem);
        break;

    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
        s_adv_op_status = param->ext_adv_data_set.status;
        xSemaphoreGive(s_adv_sem);
        break;

    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
        s_adv_op_status = param->ext_adv_start.status;
        xSemaphoreGive(s_adv_sem);
        break;

    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
        s_adv_op_status = param->ext_adv_stop.status;
        xSemaphoreGive(s_adv_sem);
        break;

    /* REQ-SEC-1: "Just Works" SMP pairing request from peer (IO_CAP=NONE) */
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "SMP Security Request from %02x:%02x:%02x:%02x:%02x:%02x -> Accepting (Just Works)",
                 param->ble_security.ble_req.bd_addr[0], param->ble_security.ble_req.bd_addr[1],
                 param->ble_security.ble_req.bd_addr[2], param->ble_security.ble_req.bd_addr[3],
                 param->ble_security.ble_req.bd_addr[4], param->ble_security.ble_req.bd_addr[5]);
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    /* REQ-SEC-1: Auto-confirm numeric comparison (No PIN prompt) */
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "SMP Numeric Comparison -> Auto-confirming (Just Works)");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        break;

    /* REQ-SEC-2: Authentication & Bonding Complete */
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Authentication SUCCESS: bonded peer %02x:%02x:%02x:%02x:%02x:%02x",
                     param->ble_security.auth_cmpl.bd_addr[0], param->ble_security.auth_cmpl.bd_addr[1],
                     param->ble_security.auth_cmpl.bd_addr[2], param->ble_security.auth_cmpl.bd_addr[3],
                     param->ble_security.auth_cmpl.bd_addr[4], param->ble_security.auth_cmpl.bd_addr[5]);
            save_last_bda_to_nvs(param->ble_security.auth_cmpl.bd_addr);
        } else {
            ESP_LOGW(TAG, "Authentication FAILED, reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGD(TAG, "Connection parameters updated: interval=%u, latency=%u, timeout=%u",
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    default:
        break;
    }
}

esp_err_t ble_audio_gap_init(const char *device_name)
{
    esp_err_t ret;

    if (device_name != nullptr && strlen(device_name) > 0) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }

    s_adv_sem = xSemaphoreCreateBinary();
    if (s_adv_sem == nullptr) {
        ESP_LOGE(TAG, "Failed to create adv semaphore");
        return ESP_ERR_NO_MEM;
    }

    /* Step 0: Ensure NVS is initialized before BT stack or PHY calibration reads */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    /* Load any previous bonded device from NVS (REQ-CONN-1) */
    load_last_bda_from_nvs();

#if !CONFIG_IDF_TARGET_ESP32S31
    /* Step 1: Release Classic BT controller memory on chips with dual-mode hardware */
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret && ret != ESP_ERR_INVALID_STATE && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Failed to release Classic BT memory: %d", ret);
        return ret;
    }
#endif

    /* Step 2: Initialize and enable BT Controller in BLE mode */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init BT controller: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable BT controller in BLE mode: %d", ret);
        return ret;
    }

    /* Step 3: Initialize and enable Bluedroid Host */
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Bluedroid: %d", ret);
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable Bluedroid: %d", ret);
        return ret;
    }

    /* Step 4: Configure Security Parameters (REQ-SEC-1, REQ-SEC-2) */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));

    /* Step 5: Register GAP callback and set device name */
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %d", ret);
        return ret;
    }

    ret = esp_ble_gap_set_device_name(s_device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GAP device name: %d", ret);
        return ret;
    }

    /* Step 6: Create 10s auto-reconnect timer (REQ-CONN-1) */
    const esp_timer_create_args_t timer_args = {
        .callback = auto_reconnect_timer_callback,
        .name = "ble_auto_reconnect",
    };
    esp_timer_create(&timer_args, &s_auto_reconnect_timer);

    build_ext_adv_payload();
    ESP_LOGI(TAG, "BLE GAP initialized successfully: device_name='%s'", s_device_name);
    return ESP_OK;
}

esp_err_t ble_audio_gap_start_advertising(void)
{
    ESP_LOGI(TAG, "Starting BLE Audio connectable Extended Advertising...");
    WAIT_ADV_OP(esp_ble_gap_ext_adv_set_params(ADV_HANDLE, &s_ext_adv_params));
    WAIT_ADV_OP(esp_ble_gap_config_ext_adv_data_raw(ADV_HANDLE, s_ext_adv_data_len, s_ext_adv_data));
    WAIT_ADV_OP(esp_ble_gap_ext_adv_start(1, s_ext_adv_inst));

    // Start auto-reconnect periodic timer if not already active
    if (s_auto_reconnect_timer != nullptr && !esp_timer_is_active(s_auto_reconnect_timer)) {
        esp_timer_start_periodic(s_auto_reconnect_timer, AUTO_RECONNECT_MS * 1000);
    }

    ESP_LOGI(TAG, "Extended Advertising active (Handle %u, Name '%s')", ADV_HANDLE, s_device_name);
    return ESP_OK;
}

esp_err_t ble_audio_gap_stop_advertising(void)
{
    WAIT_ADV_OP(esp_ble_gap_ext_adv_stop(1, s_ext_adv_stop_inst));
    ESP_LOGI(TAG, "Extended Advertising stopped");
    return ESP_OK;
}

void ble_audio_gap_set_conn_cb(ble_audio_conn_state_cb_t cb, void *user_data)
{
    s_conn_cb = cb;
    s_conn_user_data = user_data;
}

bool ble_audio_gap_is_connected(void)
{
    return s_is_connected;
}

bool ble_audio_gap_get_last_peer_bda(uint8_t bda[6])
{
    if (s_is_connected) {
        memcpy(bda, s_connected_bda, 6);
        return true;
    }
    if (s_has_bonded_bda) {
        memcpy(bda, s_last_bonded_bda, 6);
        return true;
    }
    return false;
}

void ble_audio_gap_on_app_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
        if (param->phy_update.status == ESP_OK) {
            ESP_LOGI(TAG, "ACL Connected / PHY updated: tx_phy=%u, rx_phy=%u",
                     param->phy_update.tx_phy, param->phy_update.rx_phy);
            s_is_connected = true;
            if (s_auto_reconnect_timer != nullptr && esp_timer_is_active(s_auto_reconnect_timer)) {
                esp_timer_stop(s_auto_reconnect_timer);
            }
            if (s_conn_cb != nullptr) {
                s_conn_cb(BLE_AUDIO_CONN_STATE_CONNECTED, s_conn_user_data);
            }
        }
        break;

    default:
        break;
    }
}
