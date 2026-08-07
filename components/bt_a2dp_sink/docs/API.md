# `bt_a2dp_sink` API Reference

## Configuration Structure

```cpp
typedef struct {
    const char *device_name;         // Bluetooth A2DP Sink broadcast device name
    struct {
        gpio_num_t bclk_pin;         // I2S Bit Clock (BCLK) GPIO pin
        gpio_num_t ws_pin;           // I2S Word Select / LRCK GPIO pin
        gpio_num_t dout_pin;         // I2S Data Out (DOUT) GPIO pin
        uint32_t sample_rate;        // Audio sample rate (Default: 44100 Hz)
        i2s_port_t port;             // I2S port number (Default: I2S_NUM_0)
    } i2s_config;
} bt_a2dp_sink_config_t;
```

## Functions & Callbacks

### Initialization & Control
- `esp_err_t bt_a2dp_sink_init(const bt_a2dp_sink_config_t *config)`
  - Initializes Bluedroid Bluetooth stack and native master I2S audio driver.
  - `@param[in] config Pointer to bt_a2dp_sink_config_t configuration structure.`
- `esp_err_t bt_a2dp_sink_start(void)`
  - Starts Bluetooth A2DP Sink discoverability and connectability.
- `void bt_a2dp_sink_play(void)`
  - Sends AVRCP Play command to connected Bluetooth peer.
- `void bt_a2dp_sink_pause(void)`
  - Sends AVRCP Pause command to connected Bluetooth peer.
- `void bt_a2dp_sink_stop(void)`
  - Sends AVRCP Stop command to connected Bluetooth peer.
- `void bt_a2dp_sink_next(void)`
  - Sends AVRCP Skip Next Track command to connected Bluetooth peer.
- `void bt_a2dp_sink_previous(void)`
  - Sends AVRCP Skip Previous Track command to connected Bluetooth peer.

### Callbacks
- `void bt_a2dp_sink_set_connection_state_cb(esp_a2d_connection_state_cb_t cb, void *user_data)`
  - Registers connection state change callback.
- `void bt_a2dp_sink_set_audio_state_cb(esp_a2d_audio_state_cb_t cb, void *user_data)`
  - Registers audio streaming state callback (Started, Suspended).
- `void bt_a2dp_sink_set_metadata_cb(void (*cb)(uint8_t id, const uint8_t *text))`
  - Registers AVRCP metadata notification callback.
- `void bt_a2dp_sink_set_play_pos_cb(void (*cb)(uint32_t play_pos), uint32_t interval_s)`
  - Registers playback position notification callback.

## Related Links
- [Component TOO](TOO.md)
- [Component README](../README.md)
