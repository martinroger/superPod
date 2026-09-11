# Apple iPod Accessory Protocol Component (`espod`)

`espod` provides a state machine and protocol parser for the Apple iPod Accessory Protocol (iAP / AAP). It supports:
- **General Lingo (`0x00`)**: Accessory identification, protocol versioning, options, authentication.
- **Simple Remote Lingo (`0x03`)**: Standard button press commands.
- **Extended Interface Lingo (`0x04`)**: Track list navigation, track metadata transmission (Title, Artist, Album, Genre, Duration), playback position tracking, and play status notifications.

---

## Subsystem Architecture

- **Core Affinity**: Processing tasks run on **Core 1 (APP_CPU)** (`CONFIG_ESPOD_TASK_CORE = 1`).
- **Universal Ingestion Task (`_rxTask`) & Transport Abstraction**:
  - Supports physical UART or direct-memory transports (such as USB Bulk OUT endpoints via `attachRxHandler()`).
  - Directly receives FreeRTOS task notifications (`pl2303_usb_set_rx_task_handle(espod.getRxTaskHandle())`) triggered by `tud_vendor_rx_cb()`, eliminating intermediate bridge tasks and saving 4 KB of RAM.
  - Ingests incoming data chunks directly into `processRawBuffer()`.
- **Stream Accumulator State Machine**:
  - Reassembles fragmented frames (including 1-byte transfers), validates preambles (`0xFF 0x55`, safely absorbing multi-`0xFF` padding), length, and two's complement checksums, safely dropping corrupted packets.
- **Dynamic Timeout & Debounce Latch**:
  - Enforces a **500 ms inter-byte timeout** (`INTERBYTE_TIMEOUT`) when an incomplete frame is mid-assembly (`_rxIncomplete == true`).
  - Enforces an **8000 ms serial idle timeout** (`SERIAL_TIMEOUT`) with a debounce latch (`serialTimedOut`), transitioning to `portMAX_DELAY` to prevent repeated state machine resetting.
- **Transport Outbound Callback**: Generated response frames are passed to an attached transport callback (`attachTxHandler()`) which forwards bytes to TinyUSB Bulk IN endpoint (`pl2303_usb_write_bytes`).

---

## Component Regression Testing

`espod` includes a self-contained, cross-platform host regression test suite located in `components/espod/test/`.
- Tests compile directly against the genuine production C++ sources (`src/esPod.cpp`, `L0x00.cpp`, `L0x03.cpp`, `L0x04.cpp`) using host `g++` or `clang++` with lightweight FreeRTOS/ESP-IDF mock headers.
- **To run the component tests directly:**
  ```bash
  cd components/espod/test
  ./run_tests.sh
  ```
- **To run from project root:**
  ```bash
  ./tests/run_tests.sh
  ```

---

## Detailed Documentation

- [Theory of Operation (TOO)](docs/TOO.md)
- [Component API Reference](docs/API.md)
- [Root Theory of Operation](../../TOO.md)
