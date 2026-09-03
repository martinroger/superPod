/**
 * @file ble_audio_vcp.h
 * @brief Volume Control Profile (VCP) Volume Renderer for superPod.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_audio_vcp_vol_cb_t)(uint8_t volume, bool muted);

/**
 * @brief Initialize and register VCP Volume Control Service (VCS) Renderer.
 */
esp_err_t ble_audio_vcp_init(void);

/**
 * @brief Get current VCP volume level (0 to 255).
 */
uint8_t ble_audio_vcp_get_volume(void);

/**
 * @brief Check if audio is currently muted via VCP.
 */
bool ble_audio_vcp_is_muted(void);

/**
 * @brief Register callback for VCP volume and mute state changes.
 */
void ble_audio_vcp_set_vol_cb(ble_audio_vcp_vol_cb_t cb);

/**
 * @brief Apply current VCP volume attenuation / mute to an array of 16-bit PCM samples.
 */
void ble_audio_vcp_apply_volume(int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif

