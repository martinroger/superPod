# TinyUSB Prolific PL2303 USB Transceiver Component (`pl2303_usb`)

Native ESP-IDF component emulating a Prolific PL2303 (`0x067B:0x2303`) USB-to-UART transceiver on the native USB-OTG peripheral of the ESP32-S3 / ESP32-S31.

---

## Subsystem Architecture

- **USB Stack Compatibility**: Updated for `esp_tinyusb` v2.0+ and ESP-IDF v6.2 (master branch).
- **Core Affinity**: Pinned to **Core 1 (APP_CPU)** (`CONFIG_TINYUSB_TASK_CORE = 1`).
- **Endpoint Pipeline**:
  - `0x81` (Interrupt IN): Status IRQ pipe (DCD, DSR, CTS, RI).
  - `0x02` (Bulk OUT): Inbound data pipeline from Host to ESP32.
  - `0x83` (Bulk IN): Outbound data pipeline from ESP32 to Host.
- **Event-Driven Task Notification**: Implements TinyUSB `tud_vendor_rx_cb()` to wake `usb_espod_bridge_task` instantly via FreeRTOS task notifications (`xTaskNotifyGive`), using zero idle CPU.

---

## Detailed Documentation

- [Theory of Operation (TOO)](docs/TOO.md)
- [Component API Reference](docs/API.md)
- [Root Theory of Operation](../../TOO.md)
