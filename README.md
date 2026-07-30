# superPod (main branch)

Single-MCU application firmware for **ESP32-S3 / ESP32-S31** integrating Apple iPod Accessory Protocol (iAP) emulation over USB-OTG and Bluetooth A2DP Audio Streaming to an external I2S DAC.

---

## Overarching Functionality

`superPod` merges three subsystem stacks into a unified firmware:
1. **Prolific PL2303 USB Emulation (`pl2303_usb`)**: Native TinyUSB vendor class emulation (`0x067B:0x2303`) running on ESP32-S3 USB-OTG, allowing car head units and docks (e.g., BMW, MINI, Audi) to recognize the ESP32 as an official iPod accessory transport interface over USB.
2. **iPod Protocol Engine (`espod`)**: Hybridized Lingo state machine processing iAP commands (L0x00 General Lingo, L0x03 Display Lingo, and L0x04 Extended Interface Lingo) for remote playback control and track metadata exchange.
3. **Bluetooth Audio Engine**: Wireless Bluetooth A2DP Sink receiving high-quality audio from mobile devices and streaming PCM output to an external I2S DAC (e.g., PCM5102A, MAX98357A), backed by `pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools`.

---

## Architecture & Subsystem Core Allocation

To guarantee real-time audio playback without buffer underflows or USB protocol timeouts, `superPod` enforces strict dual-core FreeRTOS task isolation via Kconfig (Option B Swapped):

| Subsystem | CPU Core | FreeRTOS Priority | Tasks / Operations |
|---|---|---|---|
| **Audio & BT** | **Core 0** (PRO_CPU) | 18–23 | Bluetooth Radio Controller (`CONFIG_BT_CTRL_PIN_TO_CORE = 0`), Bluedroid Host Stack (`CONFIG_BT_BLUEDROID_PIN_TO_CORE = 0`), A2DP SBC/AAC Audio Decoding, I2S DMA Stream |
| **USB & iPod Logic** | **Core 1** (APP_CPU) | 2–15 | TinyUSB Device Stack Task (`CONFIG_TINYUSB_TASK_CORE = 1`), In-Memory USB <-> `espod` Ringbuffer Bridge, iAP Command Processing |

---

## Hardware & Resource Requirements

- **Microcontroller**: **ESP32-S3 / ESP32-S31** (USB-OTG hardware controller required).
- **Toolchain**: ESP-IDF v6.1+ / v6.2 (master branch) using `eim` wrapper.
- **External DAC**: Standard I2S DAC (PCM5102A / MAX98357A). No ES8388 codec board required.
- **Default Pinout**:
  - **USB-OTG**: GPIO 19 (`USB D-`), GPIO 20 (`USB D+`)
  - **I2S DAC**: BCLK: `GPIO 27`, WS/LRCK: `GPIO 25`, DOUT: `GPIO 26` (Configurable via `main/Kconfig.projbuild`)
- **Flash / Memory Partitioning**: Custom partition scheme (`partitions.csv`).

---

## Implementation Quirks

- **External C++ Audio Dependencies**: Sourced automatically via `main/idf_component.yml` (`pschatzmann/ESP32-A2DP` and `pschatzmann/arduino-audio-tools`). Uses `audio_tools_compat.h` for seamless initialization.
- **Direct USB Ingestion**: USB Bulk OUT packets from the host car stereo are passed directly into `espod` raw message ringbuffers in internal RAM, bypassing serial UART overhead in single-MCU mode.
- **`esp_tinyusb` v2.0+ Migration**: Updated for ESP-IDF v6.2 API changes with explicit PHY configuration (`phy.skip_setup`), runtime configuration descriptors, and DTR/RTS bitbanging.

---

## Quick Start: Cloning & Building

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/your-org/superPod.git
   cd superPod
   git checkout main
   ```

2. **Build Firmware**:
   ```bash
   eim idf.py build
   ```

3. **Flash & Monitor**:
   ```bash
   eim idf.py -p /dev/ttyACM0 flash monitor
   ```
