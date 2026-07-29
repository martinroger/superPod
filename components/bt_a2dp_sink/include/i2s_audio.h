/**
 * @file i2s_audio.h
 * @brief Native ESP-IDF I2S Master TX Audio Output Driver Wrapper.
 * 
 * Provides standard master transmitter setup and PCM DMA stream write functions
 * using ESP-IDF esp_driver_i2s API for ESP32-S31 audio output.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Configuration structure for I2S audio output.
typedef struct {
    gpio_num_t bclk_pin;     ///< Bit Clock pin (BCLK)
    gpio_num_t ws_pin;       ///< Word Select / LR Clock pin (WS/LRCK)
    gpio_num_t dout_pin;     ///< Serial Data Output pin (DOUT)
    uint32_t sample_rate;    ///< Audio sample rate in Hz (e.g. 44100)
    int port;                ///< I2S hardware port instance (e.g. I2S_NUM_0 / 0)
} i2s_audio_config_t;

/**
 * @brief Initializes the I2S TX channel in master standard mode.
 * 
 * @param config Pointer to I2S pin and audio parameter configuration.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2s_audio_init(const i2s_audio_config_t *config);

/**
 * @brief Writes PCM audio samples to the I2S DMA transmitter buffer.
 * 
 * @param data Pointer to PCM audio byte buffer.
 * @param size Length of data buffer in bytes.
 * @return size_t Number of bytes written to I2S DMA.
 */
size_t i2s_audio_write(const uint8_t *data, size_t size);

/**
 * @brief Updates the I2S clock sample rate dynamically.
 * 
 * @param sample_rate New sample rate in Hz (e.g. 44100, 48000).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2s_audio_set_sample_rate(uint32_t sample_rate);

/**
 * @brief Deinitializes and disables the I2S TX channel.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2s_audio_deinit(void);

#ifdef __cplusplus
}
#endif
