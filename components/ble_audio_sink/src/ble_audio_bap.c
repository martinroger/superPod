/**
 * @file ble_audio_bap.c
 * @brief BAP Unicast Sink Profile, Dedicated LC3 Decoder Task & Stereo Management (REQ-AUD, REQ-FLOW).
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_ble_audio_defs.h"
#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_pacs_api.h"
#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_codec_api.h"
#include "esp_ble_audio_lc3_defs.h"
#include "esp_ble_audio_tmap_api.h"
#include "lc3.h"

#include "ble_audio_i2s.h"
#include "ble_audio_gap.h"
#include "ble_audio_bap.h"
#include "ble_audio_vcp.h"
#include "ble_audio_mcc.h"

static const char *TAG = "BLE_AUDIO_BAP";

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#define SINK_LOCATION           ((esp_ble_audio_location_t)(ESP_BLE_AUDIO_LOCATION_FRONT_LEFT | ESP_BLE_AUDIO_LOCATION_FRONT_RIGHT))
#define SOURCE_LOCATION         ((esp_ble_audio_location_t)(ESP_BLE_AUDIO_LOCATION_FRONT_LEFT | ESP_BLE_AUDIO_LOCATION_FRONT_RIGHT))

#define SINK_CONTEXT            ((esp_ble_audio_context_t)(ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED | \
                                 ESP_BLE_AUDIO_CONTEXT_TYPE_CONVERSATIONAL | \
                                 ESP_BLE_AUDIO_CONTEXT_TYPE_MEDIA))

#define SOURCE_CONTEXT          ((esp_ble_audio_context_t)(ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED | \
                                 ESP_BLE_AUDIO_CONTEXT_TYPE_CONVERSATIONAL | \
                                 ESP_BLE_AUDIO_CONTEXT_TYPE_MEDIA))

#define MAX_SINK_STREAMS        2
#define MAX_SOURCE_STREAMS      1

#define LC3_QUEUE_LEN           32
#define MAX_LC3_FRAME_BYTES     320
#define MAX_PCM_FRAME_SAMPLES   960

typedef struct {
    uint8_t stream_idx;
    uint16_t len;
    uint8_t data[MAX_LC3_FRAME_BYTES];
} lc3_frame_msg_t;

static QueueHandle_t s_lc3_queue = NULL;
static TaskHandle_t s_decode_task_handle = NULL;

static ble_audio_state_cb_t s_audio_state_cb = NULL;
static void *s_audio_state_user_data = NULL;
static ble_audio_metadata_cb_t s_metadata_cb = NULL;
static ble_audio_play_pos_cb_t s_play_pos_cb = NULL;
static ble_audio_conn_state_cb_t s_conn_cb = NULL;
static void *s_conn_user_data = NULL;

static bool s_is_streaming = false;

/* Codec Capabilities for LC3 (REQ-AUD-1, High-Fidelity Stereo) */
static uint8_t s_codec_data[] =
    ESP_BLE_AUDIO_CODEC_CAP_LC3_DATA(
        ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ | ESP_BLE_AUDIO_CODEC_CAP_FREQ_44KHZ, /* High-Fidelity 48kHz & 44.1kHz */
        ESP_BLE_AUDIO_CODEC_CAP_DURATION_10 | ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5,
        ESP_BLE_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(2),  /* Stereo supported */
        40,                                             /* Min octets per frame (32 kbps) */
        155,                                            /* Max octets per frame (High Quality 48_6_1 / 48_4_2) */
        2);                                             /* Max 2 frames per SDU */

static uint8_t s_codec_meta[] =
    ESP_BLE_AUDIO_CODEC_CAP_LC3_META(SINK_CONTEXT | SOURCE_CONTEXT);

static const esp_ble_audio_codec_cap_t s_codec_cap =
    ESP_BLE_AUDIO_CODEC_CAP_LC3(s_codec_data, s_codec_meta);

static esp_ble_audio_pacs_cap_t s_cap_sink = {
    .codec_cap = &s_codec_cap,
};

static esp_ble_audio_pacs_cap_t s_cap_source = {
    .codec_cap = &s_codec_cap,
};

static struct {
    esp_ble_audio_bap_stream_t stream;
    uint32_t sample_rate_hz;
    uint32_t frame_dur_us;
    esp_ble_audio_location_t location;
    uint16_t octets_per_frame;
    uint8_t channels;
    lc3_decoder_t dec_left;
    lc3_decoder_mem_48k_t dec_mem_left;
    lc3_decoder_t dec_right;
    lc3_decoder_mem_48k_t dec_mem_right;
    bool decoder_initialized;
} s_sink_streams[MAX_SINK_STREAMS];

static struct {
    esp_ble_audio_bap_stream_t stream;
} s_source_streams[MAX_SOURCE_STREAMS];

static const esp_ble_audio_bap_qos_cfg_pref_t s_qos_pref =
    ESP_BLE_AUDIO_BAP_QOS_CFG_PREF(true,                /* Unframed PDUs */
                                   ESP_BLE_ISO_PHY_2M,  /* Target 2M PHY */
                                   2,                   /* Retransmissions */
                                   10,                  /* Max Transport Latency (10ms) */
                                   20000,               /* Min Presentation Delay (20ms) */
                                   40000,               /* Max Presentation Delay (40ms) */
                                   20000,               /* Preferred Min Delay */
                                   40000);              /* Preferred Max Delay */

static esp_ble_audio_dir_t stream_dir(const esp_ble_audio_bap_stream_t *stream)
{
    for (size_t i = 0; i < ARRAY_SIZE(s_source_streams); i++) {
        if (stream == &s_source_streams[i].stream) {
            return ESP_BLE_AUDIO_DIR_SOURCE;
        }
    }
    return ESP_BLE_AUDIO_DIR_SINK;
}

static esp_ble_audio_bap_stream_t *stream_alloc(esp_ble_audio_dir_t dir)
{
    if (dir == ESP_BLE_AUDIO_DIR_SOURCE) {
        for (size_t i = 0; i < ARRAY_SIZE(s_source_streams); i++) {
            if (s_source_streams[i].stream.conn == NULL) {
                return &s_source_streams[i].stream;
            }
        }
    } else {
        for (size_t i = 0; i < ARRAY_SIZE(s_sink_streams); i++) {
            if (s_sink_streams[i].stream.conn == NULL) {
                return &s_sink_streams[i].stream;
            }
        }
    }
    return NULL;
}

static int16_t s_pcm_left[MAX_PCM_FRAME_SAMPLES];
static int16_t s_pcm_right[MAX_PCM_FRAME_SAMPLES];
static int16_t s_pcm_stereo[MAX_PCM_FRAME_SAMPLES * 2];

/**
 * @brief Dedicated High-Priority LC3 Audio Decoding Task (offloads iso_task).
 */
static void ble_audio_decode_task(void *arg)
{
    lc3_frame_msg_t msg;

    ESP_LOGI(TAG, "Dedicated LC3 Audio Decode Task running on Core %d", xPortGetCoreID());

    while (1) {
        if (xQueueReceive(s_lc3_queue, &msg, portMAX_DELAY) == pdTRUE) {
            uint8_t idx = msg.stream_idx;
            if (idx >= MAX_SINK_STREAMS) {
                continue;
            }

            uint32_t dur = s_sink_streams[idx].frame_dur_us > 0 ? s_sink_streams[idx].frame_dur_us : 10000;
            uint32_t rate = s_sink_streams[idx].sample_rate_hz > 0 ? s_sink_streams[idx].sample_rate_hz : 48000;
            uint16_t octets = s_sink_streams[idx].octets_per_frame > 0 ? s_sink_streams[idx].octets_per_frame : 120;

            if (!s_sink_streams[idx].decoder_initialized || s_sink_streams[idx].dec_left == NULL) {
                s_sink_streams[idx].dec_left = lc3_setup_decoder((int)dur, (int)rate, 0, &s_sink_streams[idx].dec_mem_left);
                s_sink_streams[idx].dec_right = lc3_setup_decoder((int)dur, (int)rate, 0, &s_sink_streams[idx].dec_mem_right);
                s_sink_streams[idx].decoder_initialized = (s_sink_streams[idx].dec_left != NULL && s_sink_streams[idx].dec_right != NULL);
            }

            if (s_sink_streams[idx].dec_left == NULL) {
                continue;
            }

            int num_samples = lc3_frame_samples((int)dur, (int)rate);
            if (num_samples <= 0 || num_samples > MAX_PCM_FRAME_SAMPLES) {
                continue;
            }

            /* Case 1: Interleaved Stereo in a single SDU (msg.len == 2 * octets_per_frame) */
            if (msg.len >= 2 * octets && s_sink_streams[idx].dec_right != NULL) {
                const uint8_t *left_frame = msg.data;
                const uint8_t *right_frame = msg.data + octets;

                int ret_l = lc3_decode(s_sink_streams[idx].dec_left, left_frame, (int)octets, LC3_PCM_FORMAT_S16, s_pcm_left, 1);
                int ret_r = lc3_decode(s_sink_streams[idx].dec_right, right_frame, (int)octets, LC3_PCM_FORMAT_S16, s_pcm_right, 1);

                if (ret_l >= 0 && ret_r >= 0) {
                    for (int s = 0; s < num_samples; s++) {
                        s_pcm_stereo[2 * s]     = s_pcm_left[s];
                        s_pcm_stereo[2 * s + 1] = s_pcm_right[s];
                    }
                    ble_audio_vcp_apply_volume(s_pcm_stereo, num_samples * 2);
                    ble_audio_i2s_write(s_pcm_stereo, num_samples * 2 * sizeof(int16_t), 2);
                }
            }
            /* Case 2: Single Channel Frame (msg.len == octets_per_frame) */
            else {
                int ret = lc3_decode(s_sink_streams[idx].dec_left, msg.data, (int)msg.len, LC3_PCM_FORMAT_S16, s_pcm_left, 1);
                if (ret >= 0) {
                    ble_audio_vcp_apply_volume(s_pcm_left, num_samples);
                    ble_audio_i2s_write(s_pcm_left, num_samples * sizeof(int16_t), 1);
                }
            }
        }
    }
}

/**
 * @brief ASCS Codec Configuration Request Callback (REQ-AUD-1, REQ-FLOW-1).
 */
static int config_cb(esp_ble_conn_t *conn,
                     const esp_ble_audio_bap_ep_t *ep,
                     esp_ble_audio_dir_t dir,
                     const esp_ble_audio_codec_cfg_t *codec_cfg,
                     esp_ble_audio_bap_stream_t **stream,
                     esp_ble_audio_bap_qos_cfg_pref_t *const pref,
                     esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    *stream = stream_alloc(dir);
    if (*stream == NULL) {
        ESP_LOGW(TAG, "No streams available for dir=%s", (dir == ESP_BLE_AUDIO_DIR_SINK) ? "SNK" : "SRC");
        *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_NO_MEM,
                                          ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
        return -ENOMEM;
    }

    *pref = s_qos_pref;

    /* REQ-FLOW-1: Extract sample rate, frame duration, and channel allocation */
    esp_ble_audio_codec_cfg_freq_t freq;
    uint32_t freq_hz = 48000;
    if (esp_ble_audio_codec_cfg_get_freq(codec_cfg, &freq) == ESP_OK) {
        esp_ble_audio_codec_cfg_freq_to_freq_hz(freq, &freq_hz);
    }

    esp_ble_audio_codec_cfg_frame_dur_t dur;
    uint32_t dur_us = 10000;
    if (esp_ble_audio_codec_cfg_get_frame_dur(codec_cfg, &dur) == ESP_OK) {
        esp_ble_audio_codec_cfg_frame_dur_to_frame_dur_us(dur, &dur_us);
    }

    esp_ble_audio_location_t loc = SINK_LOCATION;
    esp_ble_audio_codec_cfg_get_chan_allocation(codec_cfg, &loc, true);

    uint16_t octets = 0;
    esp_ble_audio_codec_cfg_get_octets_per_frame(codec_cfg, &octets);
    if (octets == 0) {
        octets = 120;
    }

    for (size_t i = 0; i < ARRAY_SIZE(s_sink_streams); i++) {
        if (*stream == &s_sink_streams[i].stream) {
            s_sink_streams[i].sample_rate_hz = freq_hz;
            s_sink_streams[i].frame_dur_us = dur_us;
            s_sink_streams[i].location = loc;
            s_sink_streams[i].octets_per_frame = octets;
            s_sink_streams[i].channels = 2; // Default stereo

            s_sink_streams[i].dec_left = lc3_setup_decoder((int)dur_us, (int)freq_hz, 0, &s_sink_streams[i].dec_mem_left);
            s_sink_streams[i].dec_right = lc3_setup_decoder((int)dur_us, (int)freq_hz, 0, &s_sink_streams[i].dec_mem_right);
            s_sink_streams[i].decoder_initialized = (s_sink_streams[i].dec_left != NULL && s_sink_streams[i].dec_right != NULL);

            ble_audio_i2s_set_sample_rate(freq_hz);
            ESP_LOGI(TAG, "==========================================================================");
            ESP_LOGI(TAG, ">>> BAP CONFIG [SNK #%zu]: rate=%lu Hz, dur=%lu us, octets=%u, loc=0x%08x <<<",
                     i, (unsigned long)freq_hz, (unsigned long)dur_us, octets, (unsigned int)loc);
            ESP_LOGI(TAG, "==========================================================================");
            break;
        }
    }

    ESP_LOGI(TAG, "BAP Unicast Config: dir=%s, freq=%lu Hz, dur=%lu us, octets=%u",
             (dir == ESP_BLE_AUDIO_DIR_SINK) ? "SNK" : "SRC",
             (unsigned long)freq_hz, (unsigned long)dur_us, octets);

    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int reconfig_cb(esp_ble_audio_bap_stream_t *stream,
                       esp_ble_audio_dir_t dir,
                       const esp_ble_audio_codec_cfg_t *codec_cfg,
                       esp_ble_audio_bap_qos_cfg_pref_t *const pref,
                       esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_CONF_UNSUPPORTED,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return -ENOEXEC;
}

static int qos_cb(esp_ble_audio_bap_stream_t *stream,
                  const esp_ble_audio_bap_qos_cfg_t *qos,
                  esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP QoS Configured: SDU=%u, Interval=%lu us", qos->sdu, (unsigned long)qos->interval);
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int enable_cb(esp_ble_audio_bap_stream_t *stream,
                     const uint8_t meta[], size_t meta_len,
                     esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Stream Enable request (meta_len=%zu)", meta_len);
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int start_cb(esp_ble_audio_bap_stream_t *stream,
                    esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Stream Start request");
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int metadata_cb(esp_ble_audio_bap_stream_t *stream,
                       const uint8_t meta[], size_t meta_len,
                       esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Metadata update (%zu bytes)", meta_len);
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int disable_cb(esp_ble_audio_bap_stream_t *stream,
                      esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Stream Disable request");
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int stop_cb(esp_ble_audio_bap_stream_t *stream,
                   esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Stream Stop request");
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static int release_cb(esp_ble_audio_bap_stream_t *stream,
                      esp_ble_audio_bap_ascs_rsp_t *rsp)
{
    ESP_LOGI(TAG, "BAP Stream Release request");
    *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS,
                                      ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
    return 0;
}

static const esp_ble_audio_bap_unicast_server_cb_t s_unicast_server_cb = {
    .config   = config_cb,
    .reconfig = reconfig_cb,
    .qos      = qos_cb,
    .enable   = enable_cb,
    .start    = start_cb,
    .metadata = metadata_cb,
    .disable  = disable_cb,
    .stop     = stop_cb,
    .release  = release_cb,
};

static void stream_enabled_cb(esp_ble_audio_bap_stream_t *stream)
{
    ESP_LOGI(TAG, "Stream Enabled -> Automatically starting Sink ASE");
    if (stream_dir(stream) == ESP_BLE_AUDIO_DIR_SINK) {
        esp_err_t err = esp_ble_audio_bap_stream_start(stream);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start sink stream: %d", err);
        }
    }
}

static void stream_started_cb(esp_ble_audio_bap_stream_t *stream)
{
    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, ">>> BLE AUDIO STREAM STARTED -> PLAYING (REQ-AUD-2) <<<");
    ESP_LOGI(TAG, "=====================================================");
    s_is_streaming = true;
    ble_audio_i2s_start();

    if (s_audio_state_cb != NULL) {
        s_audio_state_cb(BLE_AUDIO_PLAYBACK_PLAYING, s_audio_state_user_data);
    }
}

static void stream_stopped_cb(esp_ble_audio_bap_stream_t *stream, uint8_t reason)
{
    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, ">>> BLE AUDIO STREAM STOPPED (reason=0x%02x) <<<", reason);
    ESP_LOGI(TAG, "=====================================================");
    s_is_streaming = false;
    ble_audio_i2s_stop();

    if (s_audio_state_cb != NULL) {
        s_audio_state_cb(BLE_AUDIO_PLAYBACK_STOPPED, s_audio_state_user_data);
    }
}

static void stream_recv_cb(esp_ble_audio_bap_stream_t *stream,
                           const esp_ble_iso_recv_info_t *info,
                           const uint8_t *data, uint16_t len)
{
    /* Fast non-blocking handoff from iso_task to decoder task */
    if (stream_dir(stream) == ESP_BLE_AUDIO_DIR_SINK && data != NULL && len > 0 && len <= MAX_LC3_FRAME_BYTES) {
        for (size_t i = 0; i < ARRAY_SIZE(s_sink_streams); i++) {
            if (stream == &s_sink_streams[i].stream) {
                if (s_lc3_queue != NULL) {
                    lc3_frame_msg_t msg;
                    msg.stream_idx = (uint8_t)i;
                    msg.len = len;
                    memcpy(msg.data, data, len);
                    xQueueSend(s_lc3_queue, &msg, 0);
                }
                break;
            }
        }
    }
}

static esp_ble_audio_bap_stream_ops_t s_stream_ops = {
    .enabled = stream_enabled_cb,
    .started = stream_started_cb,
    .stopped = stream_stopped_cb,
    .recv    = stream_recv_cb,
};

static void iso_gap_app_cb(esp_ble_audio_gap_app_event_t *event)
{
    switch (event->type) {
    case ESP_BLE_AUDIO_GAP_EVENT_ACL_CONNECT:
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> BLE AUDIO PEER CONNECTED (ACL Handle: %u) <<<", event->acl_connect.conn_handle);
        ESP_LOGI(TAG, "==================================================");
        if (s_conn_cb != NULL) {
            s_conn_cb(BLE_AUDIO_CONN_STATE_CONNECTED, s_conn_user_data);
        }
        break;

    case ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT:
        ESP_LOGI(TAG, "=====================================================");
        ESP_LOGI(TAG, ">>> BLE AUDIO PEER DISCONNECTED (ACL Handle: %u, Reason: 0x%02x) <<<",
                 event->acl_disconnect.conn_handle, event->acl_disconnect.reason);
        ESP_LOGI(TAG, "=====================================================");
        s_is_streaming = false;
        ble_audio_i2s_stop();
        ble_audio_mcc_on_peer_disconnected(event->acl_disconnect.conn_handle);
        if (s_conn_cb != NULL) {
            s_conn_cb(BLE_AUDIO_CONN_STATE_DISCONNECTED, s_conn_user_data);
        }
        ble_audio_gap_start_advertising();
        break;

    case ESP_BLE_AUDIO_GAP_EVENT_SECURITY_CHANGE:
        ESP_LOGI(TAG, "BLE Audio Security updated: handle %u level %u bonded %u",
                 event->security_change.conn_handle,
                 event->security_change.sec_level,
                 event->security_change.bonded);
        break;

    default:
        break;
    }
}

static void iso_gatt_app_cb(esp_ble_audio_gatt_app_event_t *event)
{
    switch (event->type) {
    case ESP_BLE_AUDIO_GATT_EVENT_GATT_MTU_CHANGE:
        ESP_LOGI(TAG, "GATT MTU updated: handle %u mtu %u",
                 event->gatt_mtu_change.conn_handle, event->gatt_mtu_change.mtu);
        if (event->gatt_mtu_change.mtu >= ESP_BLE_AUDIO_ATT_MTU_MIN) {
            ESP_LOGI(TAG, "Starting GATT Service Discovery on peer (handle %u)...",
                     event->gatt_mtu_change.conn_handle);
            esp_err_t ret = esp_ble_audio_gattc_disc_start(event->gatt_mtu_change.conn_handle);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to start GATT discovery: %d", ret);
            }
        }
        break;

    case ESP_BLE_AUDIO_GATT_EVENT_GATTC_DISC_CMPL:
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> GATT SERVICE DISCOVERY COMPLETE: handle %u status %u <<<",
                 event->gattc_disc_cmpl.conn_handle, event->gattc_disc_cmpl.status);
        ESP_LOGI(TAG, "==================================================");
        if (event->gattc_disc_cmpl.status == 0) {
            ble_audio_mcc_on_peer_connected(event->gattc_disc_cmpl.conn_handle);
        }
        break;

    default:
        break;
    }
}

esp_err_t ble_audio_bap_init(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing BLE Audio Common & BAP Unicast Sink...");

    /* Step 0: Create dedicated LC3 Frame Queue & Audio Decode Task */
    if (s_lc3_queue == NULL) {
        s_lc3_queue = xQueueCreate(LC3_QUEUE_LEN, sizeof(lc3_frame_msg_t));
        if (s_lc3_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create LC3 frame queue");
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_decode_task_handle == NULL) {
        BaseType_t res = xTaskCreatePinnedToCore(ble_audio_decode_task,
                                                "ble_lc3_dec",
                                                8192,
                                                NULL,
                                                configMAX_PRIORITIES - 3,
                                                &s_decode_task_handle,
                                                0);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "Failed to create ble_lc3_dec task");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Step 1: Initialize BLE Audio common stack */
    esp_ble_audio_init_info_t init_info = {
        .gap_cb  = iso_gap_app_cb,
        .gatt_cb = iso_gatt_app_cb,
    };
    err = esp_ble_audio_common_init(&init_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE Audio common: %d", err);
        return err;
    }

    /* Step 2: Register PACS service (REQ-AUD-1) */
    const esp_ble_audio_pacs_register_param_t pacs_param = {
        .snk_pac = true,
        .snk_loc = true,
        .src_pac = true,
        .src_loc = true,
    };
    err = esp_ble_audio_pacs_register(&pacs_param);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PACS: %d", err);
        return err;
    }

    /* Step 3: Register BAP Unicast Server */
    esp_ble_audio_bap_unicast_server_register_param_t server_param = {
        .snk_cnt = MAX_SINK_STREAMS,
        .src_cnt = MAX_SOURCE_STREAMS,
    };
    err = esp_ble_audio_bap_unicast_server_register(&server_param);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register BAP Unicast Server: %d", err);
        return err;
    }

    err = esp_ble_audio_bap_unicast_server_register_cb(&s_unicast_server_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register unicast server callbacks: %d", err);
        return err;
    }

    /* Step 4: Register PACS Capabilities for Sink & Source */
    err = esp_ble_audio_pacs_cap_register(ESP_BLE_AUDIO_DIR_SINK, &s_cap_sink);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Sink PACS capability: %d", err);
        return err;
    }

    err = esp_ble_audio_pacs_cap_register(ESP_BLE_AUDIO_DIR_SOURCE, &s_cap_source);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Source PACS capability: %d", err);
        return err;
    }

    /* Step 5: Register stream callbacks */
    for (size_t i = 0; i < ARRAY_SIZE(s_sink_streams); i++) {
        esp_ble_audio_bap_stream_cb_register(&s_sink_streams[i].stream, &s_stream_ops);
    }
    for (size_t i = 0; i < ARRAY_SIZE(s_source_streams); i++) {
        esp_ble_audio_bap_stream_cb_register(&s_source_streams[i].stream, &s_stream_ops);
    }

    /* Step 6: Set PACS Location (MANDATORY stereo FRONT_LEFT | FRONT_RIGHT for S24/Pixel/iPhone) */
    err = esp_ble_audio_pacs_set_location(ESP_BLE_AUDIO_DIR_SINK, SINK_LOCATION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Sink PACS location: %d", err);
        return err;
    }
    err = esp_ble_audio_pacs_set_location(ESP_BLE_AUDIO_DIR_SOURCE, SOURCE_LOCATION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Source PACS location: %d", err);
        return err;
    }

    /* Step 7: Set PACS Supported & Available Contexts */
    err = esp_ble_audio_pacs_set_supported_contexts(ESP_BLE_AUDIO_DIR_SINK, SINK_CONTEXT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Sink supported contexts: %d", err);
        return err;
    }
    err = esp_ble_audio_pacs_set_available_contexts(ESP_BLE_AUDIO_DIR_SINK, SINK_CONTEXT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Sink available contexts: %d", err);
        return err;
    }

    /* Step 8: Register TMAP role as UMR (Unicast Media Receiver) */
    err = esp_ble_audio_tmap_register(ESP_BLE_AUDIO_TMAP_ROLE_UMR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register TMAP role: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "BAP Unicast Sink, PACS, TMAP UMR, and LC3 Decode Task initialized successfully");
    return ESP_OK;
}

esp_err_t ble_audio_bap_start(void)
{
    ESP_LOGI(TAG, "Starting BLE Audio Common subsystem...");
    return esp_ble_audio_common_start(NULL);
}

void ble_audio_bap_set_audio_state_cb(ble_audio_state_cb_t cb, void *user_data)
{
    s_audio_state_cb = cb;
    s_audio_state_user_data = user_data;
}

void ble_audio_bap_set_metadata_cb(ble_audio_metadata_cb_t cb)
{
    s_metadata_cb = cb;
}

void ble_audio_bap_set_play_pos_cb(ble_audio_play_pos_cb_t cb)
{
    s_play_pos_cb = cb;
}

bool ble_audio_bap_is_streaming(void)
{
    return s_is_streaming;
}
