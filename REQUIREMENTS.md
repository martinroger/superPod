# superPod Project Requirements

## Core Architecture & Goals
- **Single-MCU Application Firmware**: Merge `ipodesp32`, `espod`, and `ESPL2303_stack` into a single ESP-IDF project (`superPod`).
- **Target Microcontroller**: **ESP32-S31** only.
- **Toolchain & Framework**: **ESP-IDF v6.2 (master branch)**.
- **IDE / Environment**: VSCode with ESP-IDF extension set up for ESP-IDF v6.2 (master).

## TinyUSB & ESP-IDF v6.2 (master) Stack Refactoring
- **First Implementation Priority**: Before integrating `espod` and A2DP, the `pl2303_usb` TinyUSB stack MUST be updated to comply with the breaking API changes introduced in `esp_tinyusb` v2.0+ / ESP-IDF v6.2.
- **`esp_tinyusb` v2.0+ API Reference**: Use the `espressif-docs` MCP tool (`search_espressif_sources`) for authoritative API migration details:
  - `tinyusb_config_t` runtime configuration parameters (descriptors, task size/priority/affinity, peripheral port, PHY parameters).
  - Explicit PHY setup (`phy.skip_setup`).
  - USB Device event callbacks (`event_cb`).

## Component & Library Sourcing
- **Dependency Management via YAML**: `esp_tinyusb` (`^2.0.0`) sourced directly via the IDF Component Manager manifest (`idf_component.yml`).
- **`bt_a2dp_sink` Component**: Native ESP-IDF A2DP Sink and I2S driver implementation contained in local IDF component `superPod/components/bt_a2dp_sink` (removing external `ESP32-A2DP` & `arduino-audio-tools` dependencies).
- **`espod` Component**: Ported into `superPod/components/espod` as a hybrid Arduino library / ESP-IDF component.

## Coding Standards & Syntax Rules
- **Syntax & Naming Preservation**: Reuse as much of the original syntax of the donor projects (`ipodesp32`, `espod`, `ESPL2303_stack`) as possible, including exact function names, class definitions, and variable names, to ensure transparent diffs and code readability.
- **Doxygen Documentation**: Provide full Doxygen comment blocks (`/// @brief`, `@param`, `@return`) for every function and method.
- **Architecture Documentation Blocks**: For any profound code structural changes, new wrappers, or transport abstraction interfaces, include a detailed architectural explanation block directly preceding the function/class definition.
- **Type Standardization (`uint8_t`)**: Convert unnecessary legacy `byte` usage to standard `uint8_t` across all files, preserving `typedef uint8_t byte;` under `#ifdef ARDUINO` only if required for Arduino API compatibility.

## Core Allocation & Dual-Subsystem Isolation (Option B Swapped)
- **Core 0 (PRO_CPU) — Dedicated Audio & Bluetooth Subsystem**:
  - Bluetooth Radio Controller (`CONFIG_BT_CTRL_PIN_TO_CORE = 0`, Priority 23)
  - Bluedroid Host Stack (`CONFIG_BT_BLUEDROID_PIN_TO_CORE = 0`, Priority 20)
  - A2DP Sink SBC/AAC Audio Processing & I2S Stream DMA Task (Priority 18)
  - AVRCP Metadata Queue Processing Task (Priority 6)
- **Core 1 (APP_CPU) — Dedicated USB & iPod Protocol Subsystem**:
  - TinyUSB Device Stack Task (`CONFIG_TINYUSB_TASK_CORE = 1`, Priority 15)
  - USB <-> `espod` Direct In-Memory Software Bridge Task (Priority 10)
  - `espod` RX, Process, TX, and Timer Tasks (Priority 2-5)

## Audio Subsystem (`bt_a2dp_sink` Local Component)
- **Native Implementation**: `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools` dependencies completely removed; replaced by local native ESP-IDF component `components/bt_a2dp_sink`.
- **I2S Audio Driver**: Native master TX I2S driver using `esp_driver_i2s` (`i2s_std` mode).
- **Audio Output**: Dedicated external DAC via standard I2S ONLY.
- **Pin Configuration**: I2S signals (BCLK, WS, DOUT) configured dynamically via Kconfig (`CONFIG_I2S_BCLK_PIN`, etc.).
- **Deprecated Features**: All references to AudioKit / ES8388 codec board configurations are strictly removed.

## USB Transceiver Subsystem (`pl2303_usb`)
- **Native USB Driver**: TinyUSB Prolific PL2303 (`0x067B:0x2303`) vendor device emulation running natively on ESP32-S31 USB-OTG.
- **Inter-Component Data Flow**: Route USB vendor EP data directly to/from `espod`'s direct raw message ringbuffers in internal single-MCU mode, with optional bridge mode to physical UART.

## Agent Operational Scope & Technical Constraints
- **Requirement Grilling**: Mandatory 3–5 question interactive interviewing during feature planning.
- **Traceability Maintenance**: Maintain explicit mapping between agent operational scope, project constraints, and source code implementations in Markdown documentation.
- **ESP-IDF v6.x Compliance**: Execute builds via `eim exec -- idf.py build` (or ESP-IDF Build MCP server).
- **Target Strapping Verification**: Verify all GPIO assignments against ESP32-S3 strapping pins (GPIO 0, 3, 45, 46).
- **Stack Allocation Floor**: Ensure all FreeRTOS tasks meet or exceed the 2048–4096 byte stack size requirement.

## Implementation Traceability Matrix

| Requirement / Constraint | Target Component / File | Implementation Details | Status |
| :--- | :--- | :--- | :--- |
| **ESP32-S3 Target Verification** | [`sdkconfig.defaults`](file:///home/martinroger/Documents/superPod/sdkconfig.defaults) | `CONFIG_IDF_TARGET="esp32s31"` | Verified |
| **ESP-IDF v6.x Execution** | [`.geminirules`](file:///home/martinroger/Documents/superPod/.geminirules) | `eim exec -- idf.py build` enforced for virtual env | Verified |
| **Strapping Pin Protection** | [`main/Kconfig.projbuild`](file:///home/martinroger/Documents/superPod/main/Kconfig.projbuild) | I2S BCLK(27), WS(25), DOUT(26); DTR(5), RTS(6) - None overlap with GPIO 0,3,45,46 | Verified |
| **Task Stack Floor (>=2048B)** | [`main/main.cpp`](file:///home/martinroger/Documents/superPod/main/main.cpp) | `usb_espod_bridge_task` (4096B), `processAVRCTask` (4096B) | Verified |
| **Native A2DP Sink & I2S** | [`components/bt_a2dp_sink`](file:///home/martinroger/Documents/superPod/components/bt_a2dp_sink) | Native `esp_driver_i2s` and Bluedroid A2DP/AVRCP implementation | Verified |
| **USB PL2303 Emulation** | [`components/pl2303_usb`](file:///home/martinroger/Documents/superPod/components/pl2303_usb) | TinyUSB Prolific PL2303 vendor device emulation | Verified |
| **iAP Lingo Engine** | [`components/espod`](file:///home/martinroger/Documents/superPod/components/espod) | Direct raw iAP message processing via `processRawBuffer` in RAM | Verified |

## Documentation & Traceability
- Maintain `REQUIREMENTS.md` with all prompt-derived constraints and architecture rules.
- Maintain `PROJECT_TRACE.md` with prompt history, technical deviations, subplans, and milestone status.

