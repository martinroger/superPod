# superPod (no-audiotools branch)

Native ESP-IDF single-MCU application firmware for **ESP32-S3 / ESP32-S31**, integrating Apple iPod Accessory Protocol (iAP) USB emulation and lightweight Bluetooth A2DP Audio Sink streaming to an external I2S DAC.

> [!TIP]
> This branch completely eliminates external C++ audio library dependencies (`arduino-audio-tools`, `ESP32-A2DP`) in favor of a local native ESP-IDF component (`components/bt_a2dp_sink`).

---

## Overarching Functionality

`superPod` combines three core subsystems into a unified ESP-IDF application:
1. **Prolific PL2303 USB Emulation (`pl2303_usb`)**: Native TinyUSB vendor class interface (`0x067B:0x2303`) running on ESP32-S3 USB-OTG, presenting an authentic iPod accessory USB hardware interface to car head units and docks (e.g., BMW, MINI, Audi).
2. **iPod Protocol Engine (`espod`)**: Modular C++ component managing iAP Lingo state machines (L0x00 General Lingo, L0x03 Display Lingo, and L0x04 Extended Interface Lingo) for playback control, track position tracking, and metadata synchronization.
3. **Native Audio Subsystem (`bt_a2dp_sink`)**: Pure ESP-IDF Bluetooth A2DP Sink using Bluedroid (`esp_a2dp`, `esp_avrc`) paired with native `esp_driver_i2s` (`i2s_std` mode) for streaming audio directly to an external I2S DAC.

---

## Architecture & Subsystem Core Allocation

To prevent race conditions, audio buffer underflows, or USB protocol timeouts, `superPod` enforces strict dual-core FreeRTOS task isolation via Kconfig (Option B Swapped):

| Subsystem | CPU Core | FreeRTOS Priority | Tasks / Operations |
|---|---|---|---|
| **Audio & BT** | **Core 0** (PRO_CPU) | 18–23 | Bluetooth Radio Controller (`CONFIG_BT_CTRL_PIN_TO_CORE = 0`), Bluedroid Host Stack (`CONFIG_BT_BLUEDROID_PIN_TO_CORE = 0`), A2DP Audio Decoding & I2S DMA Task |
| **USB & iPod Logic** | **Core 1** (APP_CPU) | 2–15 | TinyUSB Task (`CONFIG_TINYUSB_TASK_CORE = 1`), USB <-> `espod` Direct In-Memory Bridge, iAP Command Processing |

---

## Hardware & Resource Requirements

- **Microcontroller**: **ESP32-S3 / ESP32-S31** (USB-OTG hardware controller required).
- **Toolchain**: ESP-IDF v6.1+ / v6.2 (master branch) using `eim` wrapper.
- **External DAC**: Standard I2S DAC (PCM5102A / MAX98357A). No ES8388 codec board required.
- **Default Pinout**:
  - **USB-OTG**: GPIO 19 (`USB D-`), GPIO 20 (`USB D+`)
  - **I2S DAC**: BCLK: `GPIO 27`, WS/LRCK: `GPIO 25`, DOUT: `GPIO 26` (Configurable via `main/Kconfig.projbuild`)
- **Flash Partitions**: Custom partition scheme (`partitions.csv`).

---

## Implementation Quirks

- **Zero External Audio Dependencies**: Managed component dependencies are limited strictly to `esp_tinyusb` (`^2.0.0`). Audio streaming is handled by `components/bt_a2dp_sink` and `i2s_audio.cpp` using official ESP-IDF drivers.
- **Direct USB-to-Protocol Bridge**: Host USB Bulk OUT endpoints pass raw iAP packets directly into `espod.processRawBuffer()` in internal RAM, bypassing serial UART overhead in single-MCU mode.
- **TinyUSB API Compliance**: Configured for `esp_tinyusb` v2.0+ / ESP-IDF v6.2 breaking changes with explicit PHY handling (`phy.skip_setup`), runtime configuration descriptors, and DTR/RTS bitbanging.

---

## Documentation & References

- [Theory of Operation (TOO) & Use Case Sequences](TOO.md)
- [Project Requirements & Agent Scope](docs/REQUIREMENTS.md)
- [Project Trace & Architecture Milestones](docs/PROJECT_TRACE.md)

### Component Documentation
- [iPod Protocol Component (`components/espod`)](components/espod/README.md)
- [PL2303 USB Transceiver Component (`components/pl2303_usb`)](components/pl2303_usb/README.md)
- [Bluetooth A2DP Sink Component (`components/bt_a2dp_sink`)](components/bt_a2dp_sink/README.md)

---

## Quick Start: Cloning & Building

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/your-org/superPod.git
   cd superPod
   git checkout no-audiotools
   ```

2. **Build Firmware**:
   ```bash
   eim run "idf.py build"
   ```

3. **Flash & Monitor**:
   ```bash
   eim run "idf.py -p /dev/ttyACM0 flash monitor"
   ```
