# `pl2303_usb` API Reference

## Functions

### Core Driver & Pin Controls
- `esp_err_t pl2303_usb_init(int task_core)`
  - Initializes TinyUSB driver stack and pins task affinity to specified core.
  - `@param[in] task_core Task core affinity for TinyUSB task (e.g. 1 for APP_CPU).`
  - `@return esp_err_t ESP_OK on success, or error code on failure.`
- `esp_err_t init_bridge_control_pins(void)`
  - Initializes DTR and RTS GPIO control pins.
  - Initializes optional DTR and RTS GPIO control pins. If `CONFIG_DTR_PIN < 0` and `CONFIG_RTS_PIN < 0`, hardware initialization is skipped and control signals operate in pure virtual mode.
  - `@return esp_err_t ESP_OK on success, or error code on failure.`
- `void pl2303_send_status(bool DCD_state, bool CTS_state, bool DSR_state, bool RI_state)`
  - `@param[in] DCD_state Data Carrier Detect status.`
  - `@param[in] CTS_state Clear to Send status.`
  - `@param[in] DSR_state Data Set Ready status.`
  - `@param[in] RI_state Ring Indicator status.`
- `void pl2303_send_status_active(void)`
  - Short-hand helper sending active PL2303 status (DCD, CTS, DSR active).

### Virtual Line Coding & Configuration Injection
- `esp_err_t pl2303_usb_set_line_coding(const pl2303_line_coding_t *coding)`
  - Manually injects or overrides virtual UART line coding parameters.
  - `@param[in] coding Pointer to line coding structure (baud rate, stop bits, parity, data bits).`
  - `@return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if coding is NULL.`
- `esp_err_t pl2303_usb_get_line_coding(pl2303_line_coding_t *coding)`
  - Retrieves active virtual line coding configuration.
  - `@param[out] coding Pointer to destination structure.`
  - `@return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if coding is NULL.`
- `void pl2303_usb_set_line_coding_callback(pl2303_line_coding_cb_t cb)`
  - Registers a callback invoked whenever the USB host updates line parameters via `SET_LINE` (0x20).
  - `@param[in] cb Callback function pointer, or NULL to disable.`
- `void pl2303_usb_get_control_lines(bool *dtr, bool *rts)`
  - Queries active virtual DTR and RTS control line states received from the USB host.
  - `@param[out] dtr Pointer to store DTR state (may be NULL).`
  - `@param[out] rts Pointer to store RTS state (may be NULL).`

### Notification & Data Transfer APIs
- `void pl2303_usb_set_rx_task_handle(TaskHandle_t task_handle)`
  - Registers FreeRTOS task handle to receive task notifications when TinyUSB receives Bulk OUT bytes.
  - `@param[in] task_handle Target task handle (or NULL to unregister).`
- `uint32_t pl2303_usb_read_bytes(uint8_t *buffer, uint32_t bufsize)`
  - Reads data received from host on Vendor Bulk OUT endpoint (`0x02`).
  - `@param[in] buffer Output buffer pointer.`
  - `@param[in] bufsize Maximum size of output buffer.`
  - `@return uint32_t Number of bytes successfully read.`
- `uint32_t pl2303_usb_write_bytes(const uint8_t *buffer, uint32_t len)`
  - Writes outbound data to host via Vendor Bulk IN endpoint (`0x83`).
  - `@param[in] buffer Input data buffer pointer.`
  - `@param[in] len Length of data in bytes.`
  - `@return uint32_t Number of bytes scheduled for transmission.`

### TinyUSB Callbacks
- `bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)`
  - Handles vendor control setup requests and line coding.
- `void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint16_t bufsize)`
  - Vendor RX callback notifying registered task via `xTaskNotifyGive()`.

## Related Links
- [Component TOO](TOO.md)
- [Component README](../README.md)
