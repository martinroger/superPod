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
6. **Milestone 5 — Native Local Component Rewrite (`no-audiotools` Branch)**:
   - Completely eliminated external managed component dependencies `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools`.
   - Implemented local native IDF component `components/bt_a2dp_sink` with `i2s_audio` sub-module using `esp_driver_i2s`.

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
  - Created `walkthrough.md` documenting completed architecture.

### Entry 8: [No-AudioTools Native Component Rewrite]
- **Date/Time**: 2026-07-29
- **Branch**: `no-audiotools`
- **Actions Taken**:
  - Removed `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools` dependencies.
  - Created local component `components/bt_a2dp_sink` incorporating `i2s_audio` module.
  - Rewrote `main/main.cpp` to use `bt_a2dp_sink` native C/C++ API.

### Entry 9: [Centralized Antigravity Rules & Skills Propagation]
- **Date/Time**: 2026-07-30
- **Branch**: `no-audiotools`
- **Actions Taken**:
  - Consolidated base (`base.geminirules`) and ESP-IDF (`esp-idf.geminirules`) rules into project `.geminirules`.
  - Added Agent Operational Scope & Implementation Traceability Matrix to `REQUIREMENTS.md`.
  - Confirmed global availability of `requirement-grilling`, `esp-development`, and `automated-meta-review` skills.
  - Audited ESP32-S3 strapping pins (GPIO 0,3,45,46) and FreeRTOS task stack size floor (>=2048B).

