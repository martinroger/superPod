/**
 * @file ble_audio_sink.h
 * @brief Bluetooth LE Audio (BAP Unicast Server / MCP Media Client) Sink Component.
 * 
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * This component encapsulates:
 *   1. BLE Security & GAP (REQ-SEC, REQ-CONN): "Just Works" SMP pairing, bond storage in NVS,
 *      connectable Extended Advertising on 1M/2M PHY, and 10s auto-reconnection.
 *   2. BAP Unicast Server (REQ-AUD): PACS (stereo FRONT_LEFT | FRONT_RIGHT), ASCS Sink endpoints,
 *      CIS Isochronous stream reception feeding the LC3 software decoder.
 *   3. I2S Flow Management (REQ-FLOW): Native esp_driver_i2s master transmitter, dynamic clock
 *      reconfiguration, mono-to-stereo expansion, and decoder backpressure.
 *   4. Media Control Profile (REQ-CTRL, REQ-META): Media Control Client (MCC/MCP) discovering
 *      remote MCS service, subscribing to Track Title, Duration, 1s Play Position, and sending
 *      Play, Pause, Stop, Next, and Previous track opcodes.
 * =================================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connection state enumeration for BLE Audio peer device.
 */
typedef enum {
    BLE_AUDIO_CONN_STATE_DISCONNECTED = 0,  /*!< No active ACL / CIS connection */
    BLE_AUDIO_CONN_STATE_CONNECTING,        /*!< ACL or CIS connection in progress */
    BLE_AUDIO_CONN_STATE_CONNECTED          /*!< ACL & Audio endpoints established */
} ble_audio_conn_state_t;

/**
 * @brief Audio streaming state enumeration.
 */
typedef enum {
    BLE_AUDIO_PLAYBACK_STOPPED = 0,         /*!< Stream stopped or inactive */
    BLE_AUDIO_PLAYBACK_PLAYING,             /*!< Stream actively receiving & decoding audio */
    BLE_AUDIO_PLAYBACK_PAUSED               /*!< Stream suspended / paused */
} ble_audio_playback_state_t;

/**
 * @brief Metadata attribute IDs for incoming media track updates.
 */
#define BLE_AUDIO_MD_ATTR_TITLE         0x01  /*!< Track title string */
#define BLE_AUDIO_MD_ATTR_ARTIST        0x02  /*!< Artist name string */
#define BLE_AUDIO_MD_ATTR_ALBUM         0x04  /*!< Album name string */
#define BLE_AUDIO_MD_ATTR_PLAYING_TIME  0x40  /*!< Total playing duration (ASCII milliseconds) */

/**
 * @brief Configuration parameters for initializing the BLE Audio Sink subsystem.
 */
typedef struct {
    const char *device_name;    /*!< Advertised local device name */
    int bclk_pin;               /*!< I2S Bit Clock (BCLK) GPIO pin */
    int ws_pin;                 /*!< I2S Word Select (WS/LRCK) GPIO pin */
    int dout_pin;               /*!< I2S Data Out (DOUT/SD) GPIO pin */
    int mclk_pin;               /*!< Optional Master Clock (MCLK) GPIO pin (-1 if unused) */
    int mute_pin;               /*!< Optional DAC Hardware Mute GPIO pin (-1 if unused) */
    uint32_t default_sample_rate; /*!< Initial I2S sample rate in Hz (e.g. 48000) */
} ble_audio_sink_config_t;

/**
 * @brief Callback function type for peer connection state changes.
 */
typedef void (*ble_audio_conn_state_cb_t)(ble_audio_conn_state_t state, void *user_data);

/**
 * @brief Callback function type for audio streaming state changes.
 */
typedef void (*ble_audio_state_cb_t)(ble_audio_playback_state_t state, void *user_data);

/**
 * @brief Callback function type for incoming track metadata attributes.
 */
typedef void (*ble_audio_metadata_cb_t)(uint8_t attr_id, const uint8_t *text);

/**
 * @brief Callback function type for periodic playback position progress notifications.
 */
typedef void (*ble_audio_play_pos_cb_t)(uint32_t play_pos_ms);

/**
 * @brief Initializes the BLE Audio Sink component and audio output driver.
 * 
 * @param[in] config Pointer to initialization configuration structure.
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_sink_init(const ble_audio_sink_config_t *config);

/**
 * @brief Starts BLE Audio advertising, services, and I2S output pipeline.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_sink_start(void);

/**
 * @brief Registers callback for peer connection state transitions.
 * 
 * @param[in] cb Callback function pointer.
 * @param[in] user_data Optional user context pointer.
 */
void ble_audio_sink_set_connection_state_callback(ble_audio_conn_state_cb_t cb, void *user_data);

/**
 * @brief Registers callback for audio playback state transitions.
 * 
 * @param[in] cb Callback function pointer.
 * @param[in] user_data Optional user context pointer.
 */
void ble_audio_sink_set_audio_state_callback(ble_audio_state_cb_t cb, void *user_data);

/**
 * @brief Registers callback for track metadata attribute updates.
 * 
 * @param[in] cb Callback function pointer.
 */
void ble_audio_sink_set_metadata_callback(ble_audio_metadata_cb_t cb);

/**
 * @brief Registers callback for periodic 1-second play position notifications.
 * 
 * @param[in] cb Callback function pointer.
 */
void ble_audio_sink_set_play_pos_callback(ble_audio_play_pos_cb_t cb);

/**
 * @brief Sends Media Control Play command to connected audio source.
 */
void ble_audio_sink_play(void);

/**
 * @brief Sends Media Control Pause command to connected audio source.
 */
void ble_audio_sink_pause(void);

/**
 * @brief Sends Media Control Stop command to connected audio source.
 */
void ble_audio_sink_stop(void);

/**
 * @brief Sends Media Control Next Track command to connected audio source.
 */
void ble_audio_sink_next(void);

/**
 * @brief Sends Media Control Previous Track command to connected audio source.
 */
void ble_audio_sink_previous(void);

/**
 * @brief Get current volume level (0-255).
 */
uint8_t ble_audio_sink_get_volume(void);

/**
 * @brief Check if audio is currently muted.
 */
bool ble_audio_sink_is_muted(void);

/**
 * @brief Register callback for volume changes.
 */
void ble_audio_sink_set_volume_callback(void (*cb)(uint8_t volume, bool muted));

#ifdef __cplusplus
}
#endif

