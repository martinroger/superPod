/**
 * @file ble_audio_mcc.h
 * @brief Media Control Profile (MCP / MCC) Client for superPod.
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
 * @brief Initialize Media Control Client (MCC) subsystem.
 */
esp_err_t ble_audio_mcc_init(void);

/**
 * @brief Notify MCC of ACL connection to begin MCS discovery.
 */
void ble_audio_mcc_on_peer_connected(uint16_t conn_handle);

/**
 * @brief Notify MCC of ACL disconnection.
 */
void ble_audio_mcc_on_peer_disconnected(uint16_t conn_handle);

/**
 * @brief Send Media Control Point command to peer (phone).
 */
esp_err_t ble_audio_mcc_send_cmd(uint8_t opcode, int32_t param);

/**
 * @brief Transport control shortcuts.
 */
esp_err_t ble_audio_mcc_play(void);
esp_err_t ble_audio_mcc_pause(void);
esp_err_t ble_audio_mcc_stop(void);
esp_err_t ble_audio_mcc_next(void);
esp_err_t ble_audio_mcc_previous(void);

/**
 * @brief Register application callbacks for metadata and playback state.
 */
void ble_audio_mcc_set_metadata_cb(ble_audio_metadata_cb_t cb);
void ble_audio_mcc_set_play_pos_cb(ble_audio_play_pos_cb_t cb);
void ble_audio_mcc_set_state_cb(ble_audio_state_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

