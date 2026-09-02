/**
 * @file ble_audio_i2s.h
 * @brief Native I2S DMA Audio Driver & Flow Manager (REQ-AUD-2, REQ-FLOW).
 *
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * This module fulfills:
 *   - REQ-AUD-2: Audio Stream Lifecycle & Hardware Mute Synchronization.
 *   - REQ-FLOW-1: Dynamic Clock & Format Reconfiguration (44.1k/48k without reboot).
 *   - REQ-FLOW-2: Channel Alignment & Mono-to-Stereo Duplication.
 *   - REQ-FLOW-3: DMA Ring Buffer Backpressure & Jitter Absorption.
 *   - REQ-FLOW-4: Hardware Mute GPIO Control (pop/click suppression).
 * =================================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int bclk_pin;
    int ws_pin;
    int dout_pin;
    int mclk_pin;
    int mute_pin;
    uint32_t default_sample_rate;
} ble_audio_i2s_config_t;

/**
 * @brief Initializes native esp_driver_i2s standard TX channel and hardware mute pin.
 * 
 * @param[in] config Pointer to I2S pin and clock configuration.
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_i2s_init(const ble_audio_i2s_config_t *config);

/**
 * @brief Enables I2S TX channel DMA.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_i2s_start(void);

/**
 * @brief Disables I2S TX channel DMA and outputs zero-fill frames to avoid pop/clicks.
 * 
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_i2s_stop(void);

/**
 * @brief Dynamically reconfigures I2S sample rate on the fly without reboot (REQ-FLOW-1).
 * 
 * @param[in] sample_rate_hz Target sample rate (e.g. 44100 or 48000).
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_i2s_set_sample_rate(uint32_t sample_rate_hz);

/**
 * @brief Writes PCM audio frames to I2S DMA with automatic mono-to-stereo duplication (REQ-FLOW-2, REQ-FLOW-3).
 * 
 * @param[in] pcm_data Pointer to 16-bit PCM samples.
 * @param[in] pcm_bytes Number of bytes in pcm_data.
 * @param[in] channels Number of channels (1 for mono, 2 for stereo).
 * @return esp_err_t ESP_OK on success, or error code.
 */
esp_err_t ble_audio_i2s_write(const void *pcm_data, size_t pcm_bytes, uint8_t channels);

/**
 * @brief Controls hardware mute GPIO pin if configured (REQ-FLOW-4).
 * 
 * @param[in] muted true to mute output, false to unmute.
 */
void ble_audio_i2s_set_mute(bool muted);

/**
 * @brief Deinitializes I2S driver and frees channel resources.
 */
void ble_audio_i2s_deinit(void);

#ifdef __cplusplus
}
#endif

