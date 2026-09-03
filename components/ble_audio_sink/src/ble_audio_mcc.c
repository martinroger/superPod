/**
 * @file ble_audio_mcc.c
 * @brief Media Control Profile (MCP / MCC) Client implementation.
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"

#include "esp_ble_audio_mcc_api.h"
#include "esp_ble_audio_mcs_defs.h"
#include "esp_timer.h"
#include "ble_audio_mcc.h"

static const char *TAG = "BLE_AUDIO_MCC";

static uint16_t s_conn_handle = 0;
static bool s_peer_connected = false;
static bool s_mcs_discovered = false;
static esp_timer_handle_t s_mcc_retry_timer = NULL;
static int s_retry_count = 0;

static ble_audio_metadata_cb_t s_metadata_cb = NULL;
static ble_audio_play_pos_cb_t s_play_pos_cb = NULL;
static ble_audio_state_cb_t s_audio_state_cb = NULL;
static void *s_audio_state_user_data = NULL;

static void mcc_retry_timer_cb(void *arg);

static void mcc_discover_mcs_cb(struct bt_conn *conn, int err)
{
    if (err != 0) {
        ESP_LOGW(TAG, "MCS discovery failed on peer: err=%d (attempt %d/5)", err, s_retry_count);
        s_mcs_discovered = false;
        if (s_peer_connected && s_retry_count < 5 && s_mcc_retry_timer != NULL) {
            s_retry_count++;
            esp_timer_stop(s_mcc_retry_timer);
            esp_timer_start_once(s_mcc_retry_timer, 1000000); /* retry in 1s */
        }
        return;
    }

    s_mcs_discovered = true;
    s_retry_count = 0;
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, ">>> MEDIA CONTROL SERVICE (MCS) DISCOVERED ON PEER <<<");
    ESP_LOGI(TAG, "==================================================");

    /* Read initial player state, track information, and capabilities */
    esp_ble_audio_mcc_read_player_name(s_conn_handle);
    esp_ble_audio_mcc_read_media_state(s_conn_handle);
    esp_ble_audio_mcc_read_track_title(s_conn_handle);
    esp_ble_audio_mcc_read_track_duration(s_conn_handle);
    esp_ble_audio_mcc_read_track_position(s_conn_handle);
    esp_ble_audio_mcc_read_opcodes_supported(s_conn_handle);
}

static char s_player_name[64] = "Media Player";

static void mcc_read_player_name_cb(struct bt_conn *conn, int err, const char *name)
{
    if (err == 0 && name != NULL) {
        strncpy(s_player_name, name, sizeof(s_player_name) - 1);
        s_player_name[sizeof(s_player_name) - 1] = '\0';
        ESP_LOGI(TAG, ">>> Media Player: \"%s\" <<<", s_player_name);
        if (s_metadata_cb != NULL) {
            /* Pass player name as Album/Source context (attr 0x03) */
            s_metadata_cb(0x03, (const uint8_t *)s_player_name);
        }
    }
}

static void mcc_track_changed_ntf_cb(struct bt_conn *conn, int err)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, ">>> MCC TRACK CHANGED NOTIFICATION RECEIVED <<<");
    ESP_LOGI(TAG, "==================================================");

    if (s_peer_connected && s_mcs_discovered) {
        esp_ble_audio_mcc_read_track_title(s_conn_handle);
        esp_ble_audio_mcc_read_track_duration(s_conn_handle);
        esp_ble_audio_mcc_read_track_position(s_conn_handle);
    }
}

static void mcc_read_track_title_cb(struct bt_conn *conn, int err, const char *title)
{
    if (err == 0 && title != NULL && strlen(title) > 0) {
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> TRACK TITLE: \"%s\" <<<", title);
        ESP_LOGI(TAG, "==================================================");

        if (s_metadata_cb != NULL) {
            const char *delim = strstr(title, " - ");
            if (delim != NULL) {
                char clean_title[128] = {0};
                char clean_artist[128] = {0};
                size_t title_len = delim - title;
                if (title_len >= sizeof(clean_title)) title_len = sizeof(clean_title) - 1;
                strncpy(clean_title, title, title_len);
                strncpy(clean_artist, delim + 3, sizeof(clean_artist) - 1);

                ESP_LOGI(TAG, ">>> Parsed -> Title: \"%s\", Artist: \"%s\" <<<", clean_title, clean_artist);
                s_metadata_cb(0x01, (const uint8_t *)clean_title);
                s_metadata_cb(0x02, (const uint8_t *)clean_artist);
            } else {
                s_metadata_cb(0x01, (const uint8_t *)title);
            }
        }
    }
}

static void mcc_read_track_duration_cb(struct bt_conn *conn, int err, int32_t dur)
{
    if (err == 0) {
        /* Duration in MCS is in centiseconds (0.01s) -> convert to milliseconds */
        uint32_t dur_ms = (dur > 0) ? ((uint32_t)dur * 10) : 0;
        ESP_LOGI(TAG, ">>> Track Duration: %lu ms (%lu s) <<<", (unsigned long)dur_ms, (unsigned long)(dur_ms / 1000));

        if (s_metadata_cb != NULL && dur_ms > 0) {
            char dur_str[16];
            snprintf(dur_str, sizeof(dur_str), "%lu", (unsigned long)dur_ms);
            /* Pass duration as Playing Time (attr 0x07) */
            s_metadata_cb(0x07, (const uint8_t *)dur_str);
        }
    }
}

static void mcc_read_track_position_cb(struct bt_conn *conn, int err, int32_t pos)
{
    if (err == 0) {
        /* Position in MCS is in centiseconds (0.01s) -> convert to milliseconds */
        uint32_t pos_ms = (pos > 0) ? ((uint32_t)pos * 10) : 0;
        ESP_LOGD(TAG, "Track Position: %lu ms", (unsigned long)pos_ms);

        if (s_play_pos_cb != NULL) {
            s_play_pos_cb(pos_ms);
        }
    }
}

static void mcc_read_media_state_cb(struct bt_conn *conn, int err, uint8_t state)
{
    if (err != 0) return;

    ESP_LOGI(TAG, ">>> MCC Media State: %u (%s) <<<",
             state,
             (state == ESP_BLE_AUDIO_MCS_MEDIA_STATE_PLAYING) ? "PLAYING" :
             (state == ESP_BLE_AUDIO_MCS_MEDIA_STATE_PAUSED)  ? "PAUSED" :
             (state == ESP_BLE_AUDIO_MCS_MEDIA_STATE_SEEKING) ? "SEEKING" : "INACTIVE");

    if (s_audio_state_cb != NULL) {
        if (state == ESP_BLE_AUDIO_MCS_MEDIA_STATE_PLAYING) {
            s_audio_state_cb(BLE_AUDIO_PLAYBACK_PLAYING, s_audio_state_user_data);
        } else if (state == ESP_BLE_AUDIO_MCS_MEDIA_STATE_PAUSED) {
            s_audio_state_cb(BLE_AUDIO_PLAYBACK_PAUSED, s_audio_state_user_data);
        } else {
            s_audio_state_cb(BLE_AUDIO_PLAYBACK_STOPPED, s_audio_state_user_data);
        }
    }
}

static void mcc_send_cmd_cb(struct bt_conn *conn, int err, const struct mpl_cmd *cmd)
{
    if (err == 0 && cmd != NULL) {
        ESP_LOGI(TAG, "MCC Command Sent successfully: opcode=0x%02x", cmd->opcode);
    } else {
        ESP_LOGW(TAG, "MCC Command Send failed: err=%d", err);
    }
}

static void mcc_cmd_ntf_cb(struct bt_conn *conn, int err, const struct mpl_cmd_ntf *ntf)
{
    if (err == 0 && ntf != NULL) {
        ESP_LOGI(TAG, "MCC Command Notification: opcode=0x%02x, result=0x%02x",
                 ntf->requested_opcode, ntf->result_code);
    }
}

static void mcc_read_opcodes_supported_cb(struct bt_conn *conn, int err, uint32_t opcodes)
{
    if (err == 0) {
        ESP_LOGI(TAG, "MCC Supported Opcodes bitmap: 0x%08lx", (unsigned long)opcodes);
    }
}

static struct bt_mcc_cb s_mcc_cb = {
    .discover_mcs             = mcc_discover_mcs_cb,
    .read_player_name         = mcc_read_player_name_cb,
    .track_changed_ntf        = mcc_track_changed_ntf_cb,
    .read_track_title         = mcc_read_track_title_cb,
    .read_track_duration      = mcc_read_track_duration_cb,
    .read_track_position      = mcc_read_track_position_cb,
    .read_media_state         = mcc_read_media_state_cb,
    .send_cmd                 = mcc_send_cmd_cb,
    .cmd_ntf                  = mcc_cmd_ntf_cb,
    .read_opcodes_supported   = mcc_read_opcodes_supported_cb,
};

static void mcc_retry_timer_cb(void *arg)
{
    if (!s_peer_connected || s_mcs_discovered) {
        return;
    }
    ESP_LOGI(TAG, ">>> Retrying MCS Discovery on peer (attempt %d/5)...", s_retry_count + 1);
    esp_err_t err = esp_ble_audio_mcc_discover_mcs(s_conn_handle, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MCS discovery attempt %d failed: err=%d", s_retry_count + 1, err);
        if (++s_retry_count < 5 && s_mcc_retry_timer != NULL) {
            esp_timer_start_once(s_mcc_retry_timer, 1000000); /* 1s */
        }
    }
}

esp_err_t ble_audio_mcc_init(void)
{
    ESP_LOGI(TAG, "Initializing Media Control Client (MCC)...");
    esp_err_t err = esp_ble_audio_mcc_init(&s_mcc_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MCC: %d", err);
        return err;
    }

    esp_timer_create_args_t timer_args = {
        .callback = mcc_retry_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mcc_retry",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &s_mcc_retry_timer);

    ESP_LOGI(TAG, "Media Control Client initialized successfully");
    return ESP_OK;
}

void ble_audio_mcc_on_peer_connected(uint16_t conn_handle)
{
    s_conn_handle = conn_handle;
    s_peer_connected = true;
    s_mcs_discovered = false;
    s_retry_count = 0;

    ESP_LOGI(TAG, "Peer connected (handle %u) -> Initiating MCS Discovery...", conn_handle);
    esp_err_t err = esp_ble_audio_mcc_discover_mcs(conn_handle, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MCS discovery initial call returned %d -> scheduling retry timer", err);
        if (s_mcc_retry_timer != NULL) {
            s_retry_count = 1;
            esp_timer_stop(s_mcc_retry_timer);
            esp_timer_start_once(s_mcc_retry_timer, 1000000);
        }
    }
}

void ble_audio_mcc_on_peer_disconnected(uint16_t conn_handle)
{
    if (s_conn_handle == conn_handle) {
        s_peer_connected = false;
        s_mcs_discovered = false;
        s_retry_count = 0;
        if (s_mcc_retry_timer != NULL) {
            esp_timer_stop(s_mcc_retry_timer);
        }
        ESP_LOGI(TAG, "Peer disconnected (handle %u) -> Resetting MCC state", conn_handle);
    }
}

esp_err_t ble_audio_mcc_send_cmd(uint8_t opcode, int32_t param)
{
    if (!s_peer_connected) {
        ESP_LOGW(TAG, "Cannot send MCC command 0x%02x: no peer connected", opcode);
        return ESP_ERR_INVALID_STATE;
    }

    esp_ble_audio_mpl_cmd_t cmd = {
        .opcode = opcode,
        .use_param = (param != 0),
        .param = param,
    };

    ESP_LOGI(TAG, ">>> Sending MCC Command: opcode=0x%02x <<<", opcode);
    return esp_ble_audio_mcc_send_cmd(s_conn_handle, &cmd);
}

esp_err_t ble_audio_mcc_play(void)
{
    return ble_audio_mcc_send_cmd(ESP_BLE_AUDIO_MCS_OPC_PLAY, 0);
}

esp_err_t ble_audio_mcc_pause(void)
{
    return ble_audio_mcc_send_cmd(ESP_BLE_AUDIO_MCS_OPC_PAUSE, 0);
}

esp_err_t ble_audio_mcc_stop(void)
{
    return ble_audio_mcc_send_cmd(ESP_BLE_AUDIO_MCS_OPC_STOP, 0);
}

esp_err_t ble_audio_mcc_next(void)
{
    return ble_audio_mcc_send_cmd(ESP_BLE_AUDIO_MCS_OPC_NEXT_TRACK, 0);
}

esp_err_t ble_audio_mcc_previous(void)
{
    return ble_audio_mcc_send_cmd(ESP_BLE_AUDIO_MCS_OPC_PREV_TRACK, 0);
}

void ble_audio_mcc_set_metadata_cb(ble_audio_metadata_cb_t cb)
{
    s_metadata_cb = cb;
}

void ble_audio_mcc_set_play_pos_cb(ble_audio_play_pos_cb_t cb)
{
    s_play_pos_cb = cb;
}

void ble_audio_mcc_set_state_cb(ble_audio_state_cb_t cb, void *user_data)
{
    s_audio_state_cb = cb;
    s_audio_state_user_data = user_data;
}
