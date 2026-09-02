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

### Entry 11: [BLE Audio Migration Feasibility & Functional Requirements Specification]
- **Date/Time**: 2026-09-01
- **Branch**: `ble-audio`
- **User Directives**: Conduct feasibility analysis for migrating from Classic BT (A2DP/AVRCP) to BLE Audio (BAP/MCP/MCS) on ESP32-S31, clarify software vs hardware codec implications (LC3), perform in-depth code review of pairing, metadata hooks, and timing behaviors, and document an implementation-agnostic requirements specification. Baseline toolchain target set to **ESP-IDF v6.1**. Clearly identify AudioTools I2S flow management capabilities in the specification.
- **Actions Taken**:
  - Created dedicated git branch `ble-audio`.
  - Performed comprehensive code review of pairing security (No-PIN / "Just Works" / SSP), auto-reconnect, auto-play on connect, metadata hooks, track-change ACK gate (`trackChangeAckPending`), and 1-second play position notifications.
  - Analyzed `AudioTools` I2S flow management capabilities (dynamic clock reconfiguration, mono-to-stereo duplication, backpressure blocking, hardware mute, bit-depth handling, buffer capacity).
  - Created and updated [`docs/FUNCTIONAL_REQUIREMENTS_BLE_AUDIO.md`](FUNCTIONAL_REQUIREMENTS_BLE_AUDIO.md) establishing the 7 requirement domains, I2S flow management breakdown, ESP-IDF v6.1 baseline, and verification criteria matrix.

### Entry 12: [BLE Audio Migration Phase 1: Clean Excision & Stack Configuration]
- **Date/Time**: 2026-09-02
- **Branch**: `ble-audio`
- **User Directives**: Cut out Classic BT and AudioTools cleanly, align sdkconfig.defaults with official ESP-IDF v6.1 BAP unicast_server and ESP32-S31 ISO defaults, establish dedicated `components/ble_audio_sink` skeleton mapping to requirement domains, refactor main orchestrator, enforce mandatory pause between phases and MCP documentation lookups.
- **Actions Taken**:
  - Excised `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools` from `main/idf_component.yml`.
  - Deleted legacy `main/audio_tools_compat.h` and cleaned C++ compile-time `-include` shims from root `CMakeLists.txt`.
  - Configured `sdkconfig.defaults` for Bluedroid host, BLE 5.0+, ISO hardware support (`CONFIG_BT_LE_ISO_SUPPORT=y`, `CONFIG_BT_BLE_FEAT_ISO_EN=y`, `CONFIG_BT_ISO_MAX_CHAN=2`), BAP Unicast Server, and PACS contexts.
  - Created `components/ble_audio_sink` (`CMakeLists.txt`, `include/ble_audio_sink.h`, `src/ble_audio_sink.cpp`) with clean C/C++ public API for GAP, BAP, I2S, and MCP callbacks.
  - Refactored `main/main.cpp` replacing A2DP and AudioTools types with `ble_audio_sink` APIs and `mediaMetadataQueue`.
  - Performed mandatory `fullclean` build check on ESP-IDF v6.1.0 (`superPod.bin` built cleanly with 0 errors, binary size reduced from ~2.2 MB to ~345 KB).

### Entry 13: [BLE Audio Migration Phase 2: BLE GAP, Security & Auto-Reconnect Implementation]
- **Date/Time**: 2026-09-02
- **Branch**: `ble-audio`
- **User Directives**: Implement REQ-SEC (Just Works pairing, no PIN/passkey, NVS bond persistence) and REQ-CONN (connectable extended advertising on 1M/2M PHY, auto-reconnect timer, connection lifecycle dispatch).
- **Actions Taken**:
  - Created `components/ble_audio_sink/include/ble_audio_gap.h` and `components/ble_audio_sink/src/ble_audio_gap.cpp`.
  - Configured BLE controller and Bluedroid host with SMP parameters (`ESP_LE_AUTH_REQ_SC_BOND`, `ESP_IO_CAP_NONE`) and auto-confirmation of numeric comparison.
  - Added early `nvs_flash_init()` sequence in `app_main` and `ble_audio_gap_init` to resolve PHY calibration and Bluetooth OSI config persistence errors (`phy_init: 0x1101`, `BT_OSI: err_code: 0x2`).
  - Configured `CONFIG_BT_CTRL_SLEEP_ENABLE=y` and `CONFIG_BT_CTRL_LP_CLK_SRC_MAIN_XTAL=y` in `sdkconfig.defaults` to eliminate the 136 kHz RC low-accuracy clock warning and guarantee < 500 ppm clock precision for ACL and ISO timing.
  - Guarded `esp_bt_controller_mem_release` on ESP32-S31 to eliminate unsupported memory release warnings.
  - Formulated connectable Extended Advertising packet on 1M/2M PHY with Flags, ASCS 0x184E service UUID, targeted audio announcement contexts (`MEDIA | CONVERSATIONAL | UNSPECIFIED`), and local device name.
  - Implemented NVS persistence for bonded peer address (`save_last_bda_to_nvs`, `load_last_bda_from_nvs`) and 10s auto-reconnect timer (`esp_timer_create`).
  - Wired `ble_audio_gap` into `ble_audio_sink.cpp`.
  - Verified compilation on ESP-IDF v6.1.0 (`superPod.bin` compiled cleanly, 0 errors, size `0x153400`).

