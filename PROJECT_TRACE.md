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
   - Configured `main/idf_component.yml` with dependencies: `esp_tinyusb: "^2.0.0"`, `pschatzmann/ESP32-A2DP`, `pschatzmann/arduino-audio-tools`.
   - Parametrized FreeRTOS task core affinities in `Kconfig.projbuild` (Option B Swapped: Core 0 for BT/A2DP, Core 1 for USB/espod).
   - Configured I2S DAC pins (BCLK 27, WS 25, DOUT 26) and A2DP Sink name.
5. **Milestone 4 — Main Application Orchestrator (`main/main.cpp`)**:
   - Built single-MCU orchestrator in `main/main.cpp` preserving donor function names (`initializeA2DPSink`, `initializeAVRCTask`, `connectionStateChanged`, `audioStateChanged`, `avrc_rn_play_pos_callback`, `avrc_metadata_callback`, `playStatusHandler`).
   - Connected USB Bulk OUT endpoint directly to `espod.processRawBuffer()`.
   - Connected `espod` playback control handlers to `a2dp_sink` player functions, and AVRCP metadata callbacks to `espod` state updates.
6. **Milestone 5 — Non-Invasive External Component Compatibility Layer**:
   - Resolved build issues with pristine `managed_components` without modifying files inside the `managed_components` directory:
     * **Fix 1 (`audio_tools_compat.h`)**: Created `main/audio_tools_compat.h` providing `using std::min; using std::max;`, pre-including FreeRTOS system headers, and providing `SOC_ADC_SAMPLE_FREQ_THRES_*` fallbacks. Forced globally for C++ files via `add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/main/audio_tools_compat.h>)` in top-level `CMakeLists.txt`.
     * **Fix 2 (`CONFIG_BT_SPP_ENABLED=y`)**: Added `CONFIG_BT_SPP_ENABLED=y` to `sdkconfig.defaults` to enable Bluedroid SPP support and expose `esp_spp_enhanced_init()`, resolving the undefined linker reference in `ESP32-A2DP`.
     * **Fix 3 (`esp_driver_i2s` requirement)**: Attached `__idf_esp_driver_i2s` and driver include paths to managed component INTERFACE targets in top-level `CMakeLists.txt`.
7. **Milestone 6 — Kconfig Refactoring & Logical Use Case Document**:
   - Transferred hardcoded `#define` remnants in `main.cpp` (`AVRC_QUEUE_SIZE`, `PROCESS_AVRC_TASK_STACK_SIZE`, `PROCESS_AVRC_TASK_PRIORITY`, `USB_ESPOD_BRIDGE_TASK_STACK_SIZE`, `USB_ESPOD_BRIDGE_TASK_PRIORITY`, `A2DP_SINK_NAME`) into `main/Kconfig.projbuild`.
   - Created [USE_CASES_AND_SEQUENCES.md](file:///home/martinroger/Documents/superPod/USE_CASES_AND_SEQUENCES.md) detailing cold boot early USB traffic, unexpected Bluetooth disconnection & auto-reconnect, USB iAP robustness (checksum errors, unplugging, high-frequency polling), rapid track skipping, and loopback prevention.
8. **Milestone 7 — Final Build Verification**:
   - Built cleanly via `eim run "idf.py build" master`.
   - Result: `superPod.bin` (size 0x1d8e00 bytes) compiled and linked cleanly with 38% free space remaining in partition.

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

### Entry 7: [Non-Invasive Component Compatibility Implementation & Build Success]
- **Date/Time**: 2026-07-29
- **Actions Taken**:
  - Reverted all manual edits inside `managed_components/` and re-fetched clean component manifests.
  - Implemented `main/audio_tools_compat.h` and registered forced inclusion in top-level `CMakeLists.txt`.
  - Added `CONFIG_BT_SPP_ENABLED=y` to `sdkconfig.defaults`.
  - Updated `partitions.csv` to 3MB app partitions.
  - Verified 100% successful compilation and linking with `eim run "idf.py build" master`.

### Entry 8: [Use Cases Document & Kconfig Parameter Refactoring]
- **Date/Time**: 2026-07-29
- **Actions Taken**:
  - Created `USE_CASES_AND_SEQUENCES.md` with Mermaid sequence diagrams.
  - Moved `#define` remnants in `main.cpp` into `main/Kconfig.projbuild`.
  - Re-verified clean 100% build execution with `eim run "idf.py build" master`.
