/**
 * @file ble_audio_bap.h
 * @brief BAP Unicast Sink Profile & Stream Lifecycle Management (REQ-AUD, REQ-FLOW).
 *
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * This module fulfills:
 *   - REQ-AUD-1: BAP Unicast Audio Server / Sink role over LE Isochronous channels.
 *                Registers PACS capabilities (Stereo FRONT_LEFT | FRONT_RIGHT) and
 *                ASCS endpoints for LC3 audio streaming.
 *   - REQ-AUD-2: Audio Stream Lifecycle & State Machine. Handles ASE state transitions
 *                (Configured -> QoS -> Enabling -> Streaming -> Disabling -> Releasing).
 *   - REQ-FLOW-1: Dynamic Clock Reconfiguration from BAP Codec Configuration.
 *   - REQ-FLOW-3: ISO Data Path & DMA Streaming backpressure.
 * =================================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ble_audio_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes BAP Unicast Sink, PACS capabilities, and stream handlers.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_bap_init(void);

/**
 * @brief Starts BLE Audio common subsystem.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_bap_start(void);

/**
 * @brief Sets the playback audio state callback dispatcher.
 * 
 * @param[in] cb Callback invoked on streaming state transitions (Playing / Paused / Stopped).
 * @param[in] user_data Context pointer passed to callback.
 */
void ble_audio_bap_set_audio_state_cb(ble_audio_state_cb_t cb, void *user_data);

/**
 * @brief Sets the metadata callback dispatcher.
 * 
 * @param[in] cb Callback invoked on incoming metadata attributes.
 */
void ble_audio_bap_set_metadata_cb(ble_audio_metadata_cb_t cb);

/**
 * @brief Sets the play position callback dispatcher.
 * 
 * @param[in] cb Callback invoked on periodic play position updates.
 */
void ble_audio_bap_set_play_pos_cb(ble_audio_play_pos_cb_t cb);

/**
 * @brief Checks if an active ISO audio stream is currently streaming.
 * 
 * @return true if streaming, false otherwise.
 */
bool ble_audio_bap_is_streaming(void);

#ifdef __cplusplus
}
#endif

