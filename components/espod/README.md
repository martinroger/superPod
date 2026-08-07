# Apple iPod Accessory Protocol Component (`espod`)

`espod` provides a state machine and protocol parser for the Apple iPod Accessory Protocol (iAP / AAP). It supports:
- **General Lingo (`0x00`)**: Accessory identification, protocol versioning, options, authentication.
- **Simple Remote Lingo (`0x03`)**: Standard button press commands.
- **Extended Interface Lingo (`0x04`)**: Track list navigation, track metadata transmission (Title, Artist, Album, Genre, Duration), playback position tracking, and play status notifications.

---

## Subsystem Architecture

- **Core Affinity**: Processing tasks run on **Core 1 (APP_CPU)** (`CONFIG_ESPOD_TASK_CORE = 1`).
- **Direct Raw Ingestion**: In single-MCU mode, incoming USB Bulk OUT packets bypass physical UART pins and enter directly via `processRawBuffer()`.
- **Transport Outbound Callback**: Generated response frames are passed to an attached transport callback (`attachTxHandler()`) which forwards bytes to TinyUSB Bulk IN endpoint (`pl2303_usb_write_bytes`).

---

## Detailed Documentation

- [Theory of Operation (TOO)](docs/TOO.md)
- [Component API Reference](docs/API.md)
- [Root Theory of Operation](../../TOO.md)
