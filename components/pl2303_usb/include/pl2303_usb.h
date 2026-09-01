/**
 * @file pl2303_usb.h
 * @brief TinyUSB Prolific PL2303 USB Transceiver Emulation Component Header.
 * 
 * Provides native USB-OTG Prolific PL2303 (0x067B:0x2303) device emulation using
 * Espressif's esp_tinyusb component (v2.0+ / ESP-IDF v6.2 master compatible).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "tinyusb.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parameterized Endpoint Addresses for PL2303 Emulation */
#ifndef CONFIG_EP_VENDOR_BULK_OUT
#define CONFIG_EP_VENDOR_BULK_OUT 0x02
#endif

#ifndef CONFIG_EP_VENDOR_BULK_IN
#define CONFIG_EP_VENDOR_BULK_IN 0x83
#endif

#ifndef CONFIG_EP_VENDOR_IRQ
#define CONFIG_EP_VENDOR_IRQ 0x81
#endif

/**
 * @brief Initializes DTR and RTS control GPIO pins.
 * @brief Prolific PL2303 / CDC UART line coding configuration.
 */
typedef struct {
    uint32_t baud_rate;  ///< Baud rate in bps (e.g. 19200, 57600, 115200)
    uint8_t stop_bits;  ///< Stop bits: 0 = 1 stop bit, 1 = 1.5 stop bits, 2 = 2 stop bits
    uint8_t parity;     ///< Parity: 0 = None, 1 = Odd, 2 = Even, 3 = Mark, 4 = Space
    uint8_t data_bits;  ///< Data bits: 5, 6, 7, 8, 16
} pl2303_line_coding_t;

/**
 * @brief Callback function type invoked when USB Host issues a SET_LINE request.
 * 
 * @param[in] coding Pointer to the applied line coding configuration.
 */
typedef void (*pl2303_line_coding_cb_t)(const pl2303_line_coding_t *coding);

/**
 * @brief Initializes optional DTR and RTS control GPIO pins.
 * 
 * If CONFIG_DTR_PIN < 0 and CONFIG_RTS_PIN < 0, physical GPIO setup is skipped
 * and control lines operate in pure virtual mode in RAM.
 * 
 * @return esp_err_t ESP_OK on success, or error code on failure.
 */
esp_err_t init_bridge_control_pins(void);

/**
 * @brief Injects or sets virtual UART line coding parameters.
 * 
 * @param[in] coding Pointer to line coding parameters to apply.
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if coding is NULL.
 */
esp_err_t pl2303_usb_set_line_coding(const pl2303_line_coding_t *coding);

/**
 * @brief Retrieves current virtual UART line coding parameters.
 * 
 * @param[out] coding Pointer to structure where current line coding will be copied.
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if coding is NULL.
 */
esp_err_t pl2303_usb_get_line_coding(pl2303_line_coding_t *coding);

/**
 * @brief Registers a callback to be notified when USB Host updates line coding via SET_LINE.
 * 
 * @param[in] cb Callback function pointer, or NULL to disable.
 */
void pl2303_usb_set_line_coding_callback(pl2303_line_coding_cb_t cb);

/**
 * @brief Retrieves current virtual control line state (DTR / RTS) set by USB host.
 * 
 * @param[out] dtr Pointer to receive DTR state (may be NULL if not needed).
 * @param[out] rts Pointer to receive RTS state (may be NULL if not needed).
 */
void pl2303_usb_get_control_lines(bool *dtr, bool *rts);

/**
 * @brief Sends Prolific PL2303 line status byte to host via Interrupt Endpoint.
 * 
 * @param DCD_state Data Carrier Detect status.
 * @param CTS_state Clear to Send status.
 * @param DSR_state Data Set Ready status.
 * @param RI_state Ring Indicator status.
 */
void pl2303_send_status(bool DCD_state, bool CTS_state, bool DSR_state, bool RI_state);

/**
 * @brief Short-hand function to send default active PL2303 status (DCD, CTS, DSR active).
 */
void pl2303_send_status_active(void);

/**
 * @brief TinyUSB Vendor Control Transfer Callback.
 * 
 * Handles Prolific PL2303 specific vendor control setup/ACK requests, line configuration
 * (baud rate, stop bits, parity, word length), and DTR/RTS GPIO signal changes.
 * 
 * @param rhport Root hub port.
 * @param stage Control transfer stage (Setup, ACK, Data).
 * @param request Pointer to USB control request structure.
 * @return true if request was handled, false otherwise.
 */
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);

/**
 * @brief Initializes the TinyUSB PL2303 Device Stack with esp_tinyusb v2.0+ parameters.
 * 
 * @param task_core FreeRTOS core affinity for the TinyUSB task.
 * @return esp_err_t ESP_OK on success, or error code on failure.
 */
esp_err_t pl2303_usb_init(int task_core);

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Sets FreeRTOS task handle to notify when USB Bulk OUT data arrives.
 * 
 * @param[in] task_handle Task handle to notify on USB RX event (or NULL to disable).
 */
void pl2303_usb_set_rx_task_handle(TaskHandle_t task_handle);

/**
 * @brief Reads data received from USB Host via Vendor Bulk OUT endpoint.
 * 
 * @param buffer Output buffer.
 * @param bufsize Size of output buffer.
 * @return uint32_t Number of bytes read.
 */
uint32_t pl2303_usb_read_bytes(uint8_t *buffer, uint32_t bufsize);

/**
 * @brief Writes data to USB Host via Vendor Bulk IN endpoint.
 * 
 * @param buffer Input data buffer.
 * @param len Length of data in bytes.
 * @return uint32_t Number of bytes successfully scheduled for transmit.
 */
uint32_t pl2303_usb_write_bytes(const uint8_t *buffer, uint32_t len);

#ifdef __cplusplus
}
#endif
