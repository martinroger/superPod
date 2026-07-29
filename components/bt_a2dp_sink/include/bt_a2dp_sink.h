/**
 * @file bt_a2dp_sink.h
 * @brief Native ESP-IDF Bluetooth Classic A2DP Sink & AVRCP Controller Interface.
 * 
 * Provides a clean C/C++ API wrapping ESP-IDF Bluedroid A2DP Sink and AVRCP Controller
 * functionality, replacing external library dependencies.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "i2s_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Callback prototype for Bluetooth A2DP connection state changes.
typedef void (*bt_a2dp_connection_state_cb_t)(esp_a2d_connection_state_t state, void *user_data);

/// @brief Callback prototype for Bluetooth A2DP audio stream state changes.
typedef void (*bt_a2dp_audio_state_cb_t)(esp_a2d_audio_state_t state, void *user_data);

/// @brief Callback prototype for AVRCP track metadata attribute updates.
typedef void (*bt_avrc_metadata_cb_t)(uint8_t attr_id, const uint8_t *text);

/// @brief Callback prototype for AVRCP track playback position notifications.
typedef void (*bt_avrc_play_pos_cb_t)(uint32_t play_pos_ms);

/// @brief Main configuration structure for Bluetooth A2DP Sink subsystem.
typedef struct {
    const char *device_name;       ///< Bluetooth local device name (e.g. "superPod-A2DP")
    i2s_audio_config_t i2s_config; ///< I2S hardware pin and audio configuration
} bt_a2dp_sink_config_t;

/**
 * @brief Initializes Bluetooth controller, Bluedroid stack, GAP, A2DP Sink, AVRCP CT, and I2S driver.
 * 
 * @param config Pointer to Bluetooth A2DP Sink and I2S configuration structure.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_init(const bt_a2dp_sink_config_t *config);

/**
 * @brief Enables GAP discoverability and starts listening for A2DP peer connection requests.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_start(void);

/**
 * @brief Registers a callback for Bluetooth connection state change events.
 * 
 * @param cb Callback function pointer.
 * @param user_data Optional user context pointer.
 */
void bt_a2dp_sink_set_connection_state_cb(bt_a2dp_connection_state_cb_t cb, void *user_data);

/**
 * @brief Registers a callback for A2DP audio stream state change events.
 * 
 * @param cb Callback function pointer.
 * @param user_data Optional user context pointer.
 */
void bt_a2dp_sink_set_audio_state_cb(bt_a2dp_audio_state_cb_t cb, void *user_data);

/**
 * @brief Registers a callback for AVRCP track metadata attribute updates.
 * 
 * @param cb Callback function pointer.
 */
void bt_a2dp_sink_set_metadata_cb(bt_avrc_metadata_cb_t cb);

/**
 * @brief Registers a callback for AVRCP playback position change notifications.
 * 
 * @param cb Callback function pointer.
 * @param interval_s Polling notification interval in seconds.
 */
void bt_a2dp_sink_set_play_pos_cb(bt_avrc_play_pos_cb_t cb, uint32_t interval_s);

/**
 * @brief Sends AVRCP Play command to connected Bluetooth peer.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_play(void);

/**
 * @brief Sends AVRCP Pause command to connected Bluetooth peer.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_pause(void);

/**
 * @brief Sends AVRCP Stop command to connected Bluetooth peer.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_stop(void);

/**
 * @brief Sends AVRCP Next Track command to connected Bluetooth peer.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_next(void);

/**
 * @brief Sends AVRCP Previous Track command to connected Bluetooth peer.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bt_a2dp_sink_previous(void);

#ifdef __cplusplus
}
#endif
