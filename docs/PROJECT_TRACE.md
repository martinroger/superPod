# superPod Project Trace & Architecture Documentation

## Project Context & Objectives
- **Target Microcontroller**: **ESP32-S31** only (native USB-OTG + BT Classic A2DP support).
- **Toolchain Target**: **ESP-IDF v6.2 (master branch)**.
- **Goal**: Merge three sub-projects into a unified single-MCU application firmware:
  1. `ESPL2303_stack`: TinyUSB emulation of Prolific PL2303 UART transceiver over USB.
  2. `espod`: Apple iPod Accessory Protocol (iAP) Lingo state machine stack.
  3. `ipodesp32`: iPod playback engine communicating with Bluetooth A2DP Sink and AVRCP metadata handlers.

## Architecture & Sub-System Strategy
1. **Toolchain & Framework Target**: ESP-IDF v6.2 (master branch).
2. **Milestone 1 — TinyUSB Stack Component (`pl2303_usb`)**:
   - Extracted TinyUSB PL2303 vendor device emulation into `components/pl2303_usb`.
   - Updated for `esp_tinyusb` v2.0+ / ESP-IDF v6.2 compatibility with runtime descriptor initialization, DTR/RTS GPIO bitbanging, and Bulk IN/OUT data functions.
3. **Milestone 2 — `espod` Hybrid Component (`components/espod`)**:
   - Refactored `espod` into a **hybrid Arduino library / ESP-IDF component**.
   - Added direct raw iAP message processing API (`processRawBuffer`) to feed USB Bulk OUT packets directly into `espod` ringbuffers in RAM.
   - Cleaned up legacy `byte` usage to standard `uint8_t`, added Doxygen comment coverage and architecture documentation blocks.
4. **Milestone 3 — Sourcing & Manifest Configuration**:
   - Configured `main/idf_component.yml` with dependencies: `esp_tinyusb: "^2.0.0"`.
   - Parametrized FreeRTOS task core affinities in `Kconfig.projbuild` (Option B Swapped: Core 0 for BT/A2DP, Core 1 for USB/espod).
   - Configured I2S DAC pins (BCLK 27, WS 25, DOUT 26) and A2DP Sink name.
5. **Milestone 4 — Main Application Orchestrator (`main/main.cpp`)**:
   - Built single-MCU orchestrator in `main/main.cpp` preserving donor function names (`initializeA2DPSink`, `initializeAVRCTask`, `connectionStateChanged`, `audioStateChanged`, `avrc_rn_play_pos_callback`, `avrc_metadata_callback`, `playStatusHandler`).
   - Connected USB Bulk OUT endpoint directly to `espod.processRawBuffer()`.
   - Connected `espod` playback control handlers to `bt_a2dp_sink` player functions, and AVRCP metadata callbacks to `espod` state updates.
6. **Milestone 5 — AudioTools & A2DP Managed Component Integration (`main` Branch)**:
   - Sourced `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools` dependencies via `main/idf_component.yml`.
   - Integrated `BluetoothA2DPSink a2dp_sink` and `I2SStream i2s` in `main/main.cpp`.
7. **Milestone 6 — Bidirectional USB Bridge & Event-Driven Notification Refactor**:
   - Added transport-agnostic `attachTxHandler(rawTxHandler_t txHandler)` callback to `esPod` so `_txTask` routes outbound response frames directly to `pl2303_usb_write_bytes()`.
   - Implemented `tud_vendor_rx_cb` TinyUSB Bulk OUT callback in `pl2303_usb` to notify `usb_espod_bridge_task` via FreeRTOS task notifications (`xTaskNotifyGive` / `ulTaskNotifyTake`), eliminating the 5ms polling loop (`vTaskDelay(5)`).
   - Added rich step-by-step logic comments across `esPod.cpp`.

## Prompts & History Log

### Entry 1: [Initial Consolidation & Planning Request]
- **Date/Time**: 2026-07-29
- **User Request Summary**: Plan transition from 3 sub-projects into a single ESP-IDF project (`superPod`).

### Entry 2: [Requirements Refinement & espod Subplan Request]
- **Date/Time**: 2026-07-29
- **User Directives**: Use ESP32-A2DP & AudioTools as IDF components, hybridize `espod`, target ESP32-S31 & ESP-IDF v6.1+, remove ES8388, maintain `REQUIREMENTS.md`.

### Entry 3: [Syntax Preservation, YAML Sourcing, Doxygen & Core Parameterization]
- **Date/Time**: 2026-07-29
- **User Directives**: Preserve syntax/names, Doxygen formatting, YAML sourcing, `uint8_t` cleanup, architecture blocks, parameterized core allocation.

### Entry 4: [Radio / Wi-Fi Core Allocation Conflict Inquiry]
- **Date/Time**: 2026-07-29
- **User Directives**: Radio core conflict risk analysis.

### Entry 5: [Swapped Core Allocation Analysis]
- **Date/Time**: 2026-07-29
- **User Directives**: Option B (Swapped: BT+I2S on Core 0, USB+espod on Core 1).

### Entry 6: [ESP-IDF v6.2 (master) & TinyUSB API Migration Requirement]
- **Date/Time**: 2026-07-29
- **User Directives**: Toolchain target set to ESP-IDF v6.2 (master) with `esp_tinyusb` v2.0+ API migration.

### Entry 7: [Execution Phase Completed]
- **Date/Time**: 2026-07-29
- **Actions Taken**:
  - Implemented `components/pl2303_usb` (TinyUSB PL2303 vendor component).
  - Implemented `components/espod` (Hybridized Apple iPod protocol engine component with direct raw iAP ingestion).
  - Configured `main/idf_component.yml`, `Kconfig.projbuild`, `sdkconfig.defaults`.
  - Implemented `main/main.cpp` single-MCU application orchestrator.

### Entry 8: [No-AudioTools Native Component Rewrite]
- **Date/Time**: 2026-07-29
- **Branch**: `no-audiotools`
- **Actions Taken**:
  - Removed `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools` dependencies.
  - Created local component `components/bt_a2dp_sink` incorporating `i2s_audio` module.
  - Rewrote `main/main.cpp` to use `bt_a2dp_sink` native C/C++ API.

### Entry 9: [Direct USB Bridge Optimization & Documentation Restructuring]
- **Date/Time**: 2026-08-07
- **Branch**: `no-audiotools`
- **Actions Taken**:
  - Enforced Doxygen `@param[in]` standards and in-function comments in `esPod.cpp`.
  - Added `attachTxHandler` to `esPod` for outbound USB frame transmission.
  - Implemented `tud_vendor_rx_cb` task notification bridge in `pl2303_usb` and `main.cpp`.
  - Reorganized root documentation (`TOO.md` at root, `REQUIREMENTS.md` and `PROJECT_TRACE.md` moved to `docs/`).
  - Generated `README.md`, `docs/TOO.md`, and `docs/API.md` for all 3 components.

### Entry 10: [PL2303 Virtualization, Line Coding Injection & Debug Logging Architecture]
- **Date/Time**: 2026-09-01
- **Branch**: `main`
- **Actions Taken**:
  - Eliminated physical UART (`UART_NUM_1`) dependency from `pl2303_usb` component.
  - Implemented in-memory host-adaptive line coding state machine (`pl2303_line_coding_t`) responding faithfully to host `SET_LINE` (0x20) and `GET_LINE` (0x21).
  - Provided external configuration injection API (`pl2303_usb_set_line_coding`, `pl2303_usb_get_line_coding`, `pl2303_usb_set_line_coding_callback`, `pl2303_usb_get_control_lines`).
  - Virtualized DTR and RTS control lines with `CONFIG_DTR_PIN` and `CONFIG_RTS_PIN` defaulting to `-1` (disabled).
  - Enabled compile-time Debug logging (`CONFIG_LOG_MAXIMUM_LEVEL=4`) with system default at `INFO` (`CONFIG_LOG_DEFAULT_LEVEL=3`), and centrally configured `PL2303_USB`, `esPod`, and `SUPERPOD_MAIN` to `ESP_LOG_DEBUG` in `main.cpp`.
### Entry 11: [Mini Cooper PCAP Analysis, Stream Accumulator Restoration & Debounced Timeouts]
- **Date/Time**: 2026-09-11
- **Branch**: `main`
- **Actions Taken**:
  - Re-analyzed 3 PCAP traces (`20260911 - Normal iPod on genuine PL2303.pcapng`, `20260911 - superPod S31 .pcapng`, `20260911 - AiO spoofed CP2102.pcapng`):
    - Confirmed USB Full-Speed (12 Mbps) operation.
    - Verified Mini Cooper head unit negotiates PL2303 baud rate to 19200 baud (8N1) via `SET_LINE` (`00 4b 00 00 00 00 08`).
    - Discovered root cause: Mini Cooper head unit sends the 19-byte iAP Identify frame (`FF FF 55 0E 00 13 00 00 00 01 00 00 00 00 00 00 00 00 DE`) fragmented into **1-byte USB Bulk OUT transfers** to Endpoint 0x02.
    - The superPod firmware dropped these 1-byte transfers because `esPod::_processPacket` expected full packets ($\ge 5$ bytes starting with `0xFF 0x55`), and the byte accumulator from `ipodesp32`'s UART task had not been ported when transitioning to direct USB memory transfers.
  - Fixed premature dead `return` statement in `components/pl2303_usb/src/pl2303_usb.cpp` (`pl2303_usb_read_bytes`).
  - Implemented the byte-stream packet accumulator directly within `components/espod` (`esPod::processRawBuffer`):
    - Hunt mode scans for preamble `0xFF 0x55`, safely absorbing arbitrary leading `0xFF` bytes (`0xFF 0xFF 0x55`).
    - Capture mode extracts length, checks validity, accumulates payload and checksum, computes two's complement checksum (`0x100 - sum(length + payload)`), and queues verified frames to `_cmdRingBuffer`.
    - Added `resetAccumulator()` and `isRxIncomplete()` API methods to `esPod`.
  - Implemented dynamic wait ticks in `usb_espod_bridge_task` (`main/main.cpp`):
    - Uses `isRxIncomplete()` to dynamically set `ulTaskNotifyTake` wait ticks to `500 ms` (`INTERBYTE_TIMEOUT`) when mid-packet.
    - If timed out mid-packet, calls `resetAccumulator()` and logs a discard warning.
    - Uses `8000 ms` (`SERIAL_TIMEOUT`) when idle. If idle timeout triggers, calls `resetState()`, logs a warning once, and latches `serialTimedOut = true` (switching wait to `portMAX_DELAY`) until new USB activity wakes the task and clears the latch.
  - Authored Generative UI interactive artifacts:
    - `usb_trace_analysis.html`: PCAP trace comparison, endpoint traffic breakdown, and SVG sequence diagram of Mini Cooper handshake.
    - `process_flow_comparison.html`: Side-by-side architecture comparison between `ipodesp32` (UART) and `superPod` (USB) ingestion flows.
  - Created standalone unit test harness (`scratch/test_accumulator.cpp`) verifying 1-byte packet assembly, duplicate sync bytes, checksum verification, and error recovery.
  - Updated all documentation (`TOO.md`, `components/espod/README.md`, `components/espod/docs/TOO.md`, `components/espod/docs/API.md`, `docs/REQUIREMENTS.md`, and `docs/PROJECT_TRACE.md`) with visual Mermaid flowcharts.
  - Performed verification build using `eim run "idf.py build" v6.1`.

### Entry 12: [Universal esPod Ingestion, Bridge Task Elimination & Component Host Testing]
- **Date/Time**: 2026-09-11
- **Branch**: `main`
- **Actions Taken**:
  - **Universal `esPod::_rxTask` Transport Ingestion**:
    - Added `rawRxHandler_t` function pointer type, `attachRxHandler()`, and `getRxTaskHandle()` to `esPod.h` and `esPod.cpp`.
    - Integrated dynamic wait-for-notification (`ulTaskNotifyTake`) directly into `esPod::_rxTask`:
      - Mid-packet inter-byte timeout (500 ms) automatically calls `resetAccumulator()` when stalled mid-frame.
      - Serial idle timeout (8000 ms) resets state machine and latches `serialTimedOut = true` (switching wait to `portMAX_DELAY` until new traffic wakes the task).
    - Attached USB transport callback directly: `espod.attachRxHandler(pl2303_usb_read_bytes)` and `pl2303_usb_set_rx_task_handle(espod.getRxTaskHandle())`.
  - **Eliminated Bridge Task & Freed 4 KB Stack**:
    - Removed `usb_espod_bridge_task` from `main/main.cpp` and deleted its Kconfig parameters (`CONFIG_USB_ESPOD_BRIDGE_TASK_PRIORITY` and `CONFIG_USB_ESPOD_BRIDGE_TASK_STACK_SIZE`), saving 4,096 bytes of task stack allocation.
  - **Relocated & Production-Source Host Test Suite**:
    - Built self-contained component test environment in `components/espod/test/` with mock ESP-IDF/FreeRTOS headers (`mock_idf/`).
    - Compiles directly against real production C++ sources (`components/espod/src/esPod.cpp`, `L0x00.cpp`, `L0x03.cpp`, `L0x04.cpp`) without mocks or duplicate code.
    - Verified 9 test suites covering:
      - 1-byte streaming (Mini Cooper emulation),
      - multi-packet contiguous streams,
      - checksum validation and rejection,
      - timeout reset and framing recovery,
      - invalid length validation,
      - mixed consolidated stream bursts (1.5 packets in Burst 1, completed by Burst 2),
      - preamble split across burst boundaries,
      - random chunk stress testing (1–7 byte slices),
      - inter-packet garbage tolerance.
    - Added cross-platform component runner (`components/espod/test/run_tests.sh`) and root umbrella runner (`tests/run_tests.sh`).
  - **Documentation & Build Verification**:
    - Updated `TOO.md`, `components/espod/README.md`, `components/espod/docs/API.md`, and `components/espod/docs/TOO.md`.
    - Rebuilt firmware with ESP-IDF v6.1 (`eim run "idf.py build" v6.1`), producing clean binary `build/superPod.bin` (0x16a2a0 bytes).
