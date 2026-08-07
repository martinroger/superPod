# Bluetooth A2DP Sink & Native I2S DAC Component (`bt_a2dp_sink`)

Native ESP-IDF C++ component executing on **Core 0 (PRO_CPU)** that manages Bluetooth Classic A2DP audio sink streaming, AVRCP media control metadata, and master I2S audio output to an external DAC.

---

## Subsystem Architecture

- **Bluetooth Core Affinity**: Bluedroid host stack and radio controller pinned to **Core 0** (`CONFIG_BT_BLUEDROID_PIN_TO_CORE = 0`, `CONFIG_BT_CTRL_PIN_TO_CORE = 0`).
- **Audio Output**: Master TX I2S driver (`esp_driver_i2s` in `i2s_std` mode) streaming SBC/AAC PCM samples directly to external I2S DAC pins (BCLK, WS, DOUT).
- **Metadata Offloading**: AVRCP track metadata notifications (Title, Artist, Album, Duration, Position) are pushed into a lock-free queue (`avrcMetadataQueue`) to decouple Bluetooth ISR execution from iAP protocol logic.

---

## Detailed Documentation

- [Theory of Operation (TOO)](docs/TOO.md)
- [Component API Reference](docs/API.md)
- [Root Theory of Operation](../../TOO.md)
