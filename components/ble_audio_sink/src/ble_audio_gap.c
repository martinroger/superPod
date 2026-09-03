/**
 * @file ble_audio_gap.c
 * @brief BLE GAP, Security & Auto-Reconnect Implementation (REQ-SEC, REQ-CONN).
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
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
#include "esp_ble_audio_common_api.h"

#include "ble_audio_gap.h"

static const char *TAG = "BLE_AUDIO_GAP";

#define NVS_NAMESPACE       "ble_gap"
#define NVS_KEY_LAST_BDA    "last_bda"
#define ADV_HANDLE          0
#define ADV_SID             0
#define ADV_TX_POWER        10
#define ADV_INTERVAL_MS     25  // 20-30ms fast connectable advertising interval for BAP
#define AUTO_RECONNECT_MS   10000

static SemaphoreHandle_t s_adv_sem = NULL;
static esp_bt_status_t s_adv_op_status = ESP_BT_STATUS_SUCCESS;
static bool s_is_connected = false;
static uint8_t s_connected_bda[6] = {0};
static uint8_t s_last_bonded_bda[6] = {0};
static bool s_has_bonded_bda = false;
static char s_device_name[32] = "superPod-Audio";
static uint8_t s_ext_adv_data[256];
static size_t s_ext_adv_data_len = 0;

static ble_audio_conn_state_cb_t s_conn_cb = NULL;
static void *s_conn_user_data = NULL;
static esp_timer_handle_t s_auto_reconnect_timer = NULL;

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
 * @brief Timer callback for periodic background reconnection to last bonded peer (REQ-CONN-1).
 */
static void auto_reconnect_timer_callback(void *arg)
{
    if (s_is_connected) {
        return;
    }

    if (s_has_bonded_bda) {
        ESP_LOGD(TAG, "Auto-reconnect timer tick: target bonded peer %02x:%02x:%02x:%02x:%02x:%02x",
                 s_last_bonded_bda[0], s_last_bonded_bda[1], s_last_bonded_bda[2],
                 s_last_bonded_bda[3], s_last_bonded_bda[4], s_last_bonded_bda[5]);
    }
}

/**
 * @brief Strictly packed compile-time structure representing the BLE Audio EIR blocks.
 * Enforces correct Bluetooth 5.x Extended Advertising AD Structure sizing before compilation.
 */
typedef struct __attribute__((packed)) {
    /* 1. Flags (AD Type 0x01, Len 2) */
    uint8_t  flags_len;
    uint8_t  flags_type;
    uint8_t  flags_val;

    /* 2. Appearance (AD Type 0x19, Len 3) */
    uint8_t  app_len;
    uint8_t  app_type;
    uint16_t app_val;

    /* 3. 16-bit Service UUIDs: ASCS, CAS, TMAS (AD Type 0x02, Len 7) */
    uint8_t  srv_len;
    uint8_t  srv_type;
    uint16_t srv_ascs;
    uint16_t srv_cas;
    uint16_t srv_tmas;

    /* 4. TMAS Service Data (AD Type 0x16, Len 5, UUID 0x1855, Role UMR 0x0008) */
    uint8_t  tmas_len;
    uint8_t  tmas_type;
    uint16_t tmas_uuid;
    uint16_t tmas_role;

    /* 5. CAS Service Data (AD Type 0x16, Len 4, UUID 0x1853, Announcement 0x01) */
    uint8_t  cas_len;
    uint8_t  cas_type;
    uint16_t cas_uuid;
    uint8_t  cas_announcement;

    /* 6. ASCS Service Data (AD Type 0x16, Len 5, UUID 0x184E, Media Context 0x0004) */
    uint8_t  ascs_len;
    uint8_t  ascs_type;
    uint16_t ascs_uuid;
    uint16_t ascs_context;
} ble_audio_eir_fixed_t;

/* --- Compile-Time EIR Verification Assertions --- */
_Static_assert(sizeof(ble_audio_eir_fixed_t) == 32, "EIR fixed payload size must be exactly 32 bytes");
_Static_assert(offsetof(ble_audio_eir_fixed_t, flags_len) == 0,  "Flags offset mismatch");
_Static_assert(offsetof(ble_audio_eir_fixed_t, app_len) == 3,    "Appearance offset mismatch");
_Static_assert(offsetof(ble_audio_eir_fixed_t, srv_len) == 7,    "Service UUIDs offset mismatch");
_Static_assert(offsetof(ble_audio_eir_fixed_t, tmas_len) == 15,  "TMAS offset mismatch");
_Static_assert(offsetof(ble_audio_eir_fixed_t, cas_len) == 21,   "CAS offset mismatch");
_Static_assert(offsetof(ble_audio_eir_fixed_t, ascs_len) == 26,  "ASCS offset mismatch");
_Static_assert(sizeof(ble_audio_eir_fixed_t) + 2 + sizeof(s_device_name) <= sizeof(s_ext_adv_data),
               "Total advertising payload exceeds s_ext_adv_data buffer");
_Static_assert(sizeof(ble_audio_eir_fixed_t) + 2 + sizeof(s_device_name) <= 251,
               "Total advertising payload exceeds BLE 5.0 max extended advertising PDU (251 bytes)");

static const ble_audio_eir_fixed_t s_eir_fixed_template = {
    .flags_len         = 2,
    .flags_type        = ESP_BLE_AD_TYPE_FLAG,
    .flags_val         = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,

    .app_len           = 3,
    .app_type          = ESP_BLE_AD_TYPE_APPEARANCE,
    .app_val           = 0x0842, // Audio Sink / Standalone Speaker (Little-endian on RISC-V)

    .srv_len           = 7,
    .srv_type          = ESP_BLE_AD_TYPE_16SRV_PART,
    .srv_ascs          = 0x184E,
    .srv_cas           = 0x1853,
    .srv_tmas          = 0x1855,

    .tmas_len          = 5,
    .tmas_type         = ESP_BLE_AD_TYPE_SERVICE_DATA,
    .tmas_uuid         = 0x1855,
    .tmas_role         = 0x0008, // ESP_BLE_AUDIO_TMAP_ROLE_UMR

    .cas_len           = 4,
    .cas_type          = ESP_BLE_AD_TYPE_SERVICE_DATA,
    .cas_uuid          = 0x1853,
    .cas_announcement  = 0x01,   // Targeted Announcement

    .ascs_len          = 5,
    .ascs_type         = ESP_BLE_AD_TYPE_SERVICE_DATA,
    .ascs_uuid         = 0x184E,
    .ascs_context      = (uint16_t)ESP_BLE_AUDIO_CONTEXT_TYPE_MEDIA,
};

/**
 * @brief Builds and rigorously validates the BLE Extended Advertising EIR payload.
 */
static void build_ext_adv_payload(void)
{
    // 1. Copy fixed compile-time verified EIR template
    memcpy(s_ext_adv_data, &s_eir_fixed_template, sizeof(s_eir_fixed_template));
    size_t offset = sizeof(s_eir_fixed_template);

    // 2. Append Complete Local Name
    size_t name_len = strlen(s_device_name);
    if (name_len > 0 && offset + 2 + name_len <= sizeof(s_ext_adv_data)) {
        s_ext_adv_data[offset++] = (uint8_t)(name_len + 1);
        s_ext_adv_data[offset++] = ESP_BLE_AD_TYPE_NAME_CMPL;
        memcpy(&s_ext_adv_data[offset], s_device_name, name_len);
        offset += name_len;
    }
    s_ext_adv_data_len = offset;

    // 3. In-code validation pass: parse every EIR block and verify framing & bounds
    size_t cur = 0;
    size_t record_count = 0;
    while (cur < s_ext_adv_data_len) {
        uint8_t rec_len = s_ext_adv_data[cur];
        assert(rec_len >= 1 && "EIR block length must be at least 1 (for AD Type)");
        assert(cur + 1 + rec_len <= s_ext_adv_data_len && "EIR block exceeds total payload length");
        uint8_t ad_type = s_ext_adv_data[cur + 1];
        ESP_LOGD(TAG, "  EIR Record #%zu: offset=%zu, len=%u, type=0x%02X", record_count++, cur, rec_len, ad_type);
        cur += (1 + rec_len);
    }
    assert(cur == s_ext_adv_data_len && "EIR records do not sum to total advertising payload length");

    ESP_LOGI(TAG, "Extended Advertising payload validated successfully (%zu records, %u bytes)", record_count, (unsigned)s_ext_adv_data_len);
    ESP_LOG_BUFFER_HEX(TAG, s_ext_adv_data, s_ext_adv_data_len);
}

/**
 * @brief Bluedroid BLE GAP event handler.
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
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> BLE PAIRING REQUEST from %02x:%02x:%02x:%02x:%02x:%02x (Just Works) <<<",
                 param->ble_security.ble_req.bd_addr[0], param->ble_security.ble_req.bd_addr[1],
                 param->ble_security.ble_req.bd_addr[2], param->ble_security.ble_req.bd_addr[3],
                 param->ble_security.ble_req.bd_addr[4], param->ble_security.ble_req.bd_addr[5]);
        ESP_LOGI(TAG, "==================================================");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    /* REQ-SEC-1: Auto-confirm numeric comparison (No PIN prompt) */
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "SMP Numeric Comparison -> Auto-confirming (Just Works)");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        break;

    /* REQ-SEC-2: Authentication & Bonding Complete */
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        /* Forward to BLE Audio library to authorize GATT Audio characteristics */
        esp_ble_audio_gap_app_post_event(event, param);
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "==================================================");
            ESP_LOGI(TAG, ">>> BLE AUTHENTICATION SUCCESS: %02x:%02x:%02x:%02x:%02x:%02x <<<",
                     param->ble_security.auth_cmpl.bd_addr[0], param->ble_security.auth_cmpl.bd_addr[1],
                     param->ble_security.auth_cmpl.bd_addr[2], param->ble_security.auth_cmpl.bd_addr[3],
                     param->ble_security.auth_cmpl.bd_addr[4], param->ble_security.auth_cmpl.bd_addr[5]);
            ESP_LOGI(TAG, "==================================================");
            save_last_bda_to_nvs(param->ble_security.auth_cmpl.bd_addr);
        } else {
            ESP_LOGW(TAG, "Authentication FAILED, reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, ">>> BLE Connection parameters updated: interval=%u, latency=%u, timeout=%u <<<",
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

    if (device_name != NULL && strlen(device_name) > 0) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }

    s_adv_sem = xSemaphoreCreateBinary();
    if (s_adv_sem == NULL) {
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

static bool s_adv_params_configured = false;

esp_err_t ble_audio_gap_start_advertising(void)
{
    ESP_LOGI(TAG, "Starting BLE Audio connectable Extended Advertising...");
    if (!s_adv_params_configured) {
        WAIT_ADV_OP(esp_ble_gap_ext_adv_set_params(ADV_HANDLE, &s_ext_adv_params));
        WAIT_ADV_OP(esp_ble_gap_config_ext_adv_data_raw(ADV_HANDLE, s_ext_adv_data_len, s_ext_adv_data));
        s_adv_params_configured = true;
    }
    WAIT_ADV_OP(esp_ble_gap_ext_adv_start(1, s_ext_adv_inst));

    // Start auto-reconnect periodic timer if not already active
    if (s_auto_reconnect_timer != NULL && !esp_timer_is_active(s_auto_reconnect_timer)) {
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
            if (s_auto_reconnect_timer != NULL && esp_timer_is_active(s_auto_reconnect_timer)) {
                esp_timer_stop(s_auto_reconnect_timer);
            }
            if (s_conn_cb != NULL) {
                s_conn_cb(BLE_AUDIO_CONN_STATE_CONNECTED, s_conn_user_data);
            }
        }
        break;

    default:
        break;
    }
}

