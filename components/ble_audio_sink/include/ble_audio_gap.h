/**
 * @file ble_audio_gap.h
 * @brief BLE GAP, Security & Auto-Reconnect Management (REQ-SEC, REQ-CONN).
 *
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * This module fulfills:
 *   - REQ-SEC-1: "Just Works" unauthenticated pairing (no static PIN / passkey).
 *                Configures SMP IO_CAP_NONE with auto-confirm for numeric comparison.
 *   - REQ-SEC-2: NVS Bond Storage. Automatically stores encryption keys, IRK, and
 *                identity addresses in non-volatile storage.
 *   - REQ-CONN-1: Auto-Reconnect on Link Loss & MCU Boot. Persists last connected
 *                peer's BDA in NVS and runs an auto-reconnect timer restarting fast
 *                connectable Extended Advertising within 10 seconds.
 *   - REQ-CONN-2: Connect Lifecycle Auto-Play. Notifies application on connection
 *                establishment to immediately resume media state.
 * =================================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "ble_audio_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes Bluetooth Controller (BLE mode), Bluedroid host, SMP Just-Works
 *        security parameters, and loads last bonded peer BDA from NVS.
 * 
 * @param[in] device_name Complete local name to advertise in Extended Advertising.
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_gap_init(const char *device_name);

/**
 * @brief Starts connectable Extended Advertising on 1M/2M PHY with ASCS service data.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_gap_start_advertising(void);

/**
 * @brief Stops active extended advertising.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_gap_stop_advertising(void);

/**
 * @brief Sets the connection state callback dispatcher for main application.
 * 
 * @param[in] cb Callback invoked on ACL connection/disconnection.
 * @param[in] user_data Context pointer passed to callback.
 */
void ble_audio_gap_set_conn_cb(ble_audio_conn_state_cb_t cb, void *user_data);

/**
 * @brief Checks if a peer device is currently connected via ACL.
 * 
 * @return true if connected, false otherwise.
 */
bool ble_audio_gap_is_connected(void);

/**
 * @brief Retrieves the Bluetooth Device Address (BDA) of the connected or last bonded peer.
 * 
 * @param[out] bda Pointer to 6-byte buffer receiving the BDA.
 * @return true if a valid BDA was populated, false otherwise.
 */
bool ble_audio_gap_get_last_peer_bda(uint8_t bda[6]);

/**
 * @brief Internal handler invoked by BLE Audio common layer on GAP ACL events.
 * 
 * @param[in] event Event type.
 * @param[in] param Event parameters.
 */
void ble_audio_gap_on_app_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

#ifdef __cplusplus
}
#endif

