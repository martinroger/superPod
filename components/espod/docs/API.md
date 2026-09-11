# `espod` API Reference

## Class Definition

`class esPod`

### Constructor & Destructor
- `esPod(uint8_t uartNum = 1, int rxPin = -1, int txPin = -1, uint32_t baud = 19200)`
  - Master constructor for esPod class.
  - `@param[in] uartNum Hardware UART port number (Default: 1).`
  - `@param[in] rxPin RX pin number (Default: -1 for unassigned/direct USB mode).`
  - `@param[in] txPin TX pin number (Default: -1 for unassigned/direct USB mode).`
  - `@param[in] baud Baudrate (Default: 19200 for Apple AAP).`
- `~esPod()`
  - Destructor freeing FreeRTOS ringbuffers, queues, tasks, and software timers.

### Core Control & Handlers
- `void resetState()`
  - Resets internal state machine variables, clears track indices, cancels pending timers, and resets the accumulator.
- `void resetAccumulator()`
  - Resets the byte stream accumulator state, clearing any partially assembled packet buffer and returning to `0xFF 0x55` preamble sync hunt mode.
- `bool isRxIncomplete() const`
  - Returns `true` if the accumulator has synced on a preamble and is actively assembling mid-packet bytes; returns `false` if idle/waiting for preamble.
- `void attachPlayControlHandler(playStatusHandler_t playHandler)`
  - `@param[in] playHandler Pointer to playback controller callback function.`
- `void attachTxHandler(rawTxHandler_t txHandler)`
  - `@param[in] txHandler Pointer to raw transport transmit callback function.`
- `void attachRxHandler(rawRxHandler_t rxHandler)`
  - `@param[in] rxHandler Pointer to raw transport receive callback function (e.g. pl2303_usb_read_bytes).`
  - When attached, `_rxTask` uses this callback to ingest bytes directly into `processRawBuffer()`.
- `TaskHandle_t getRxTaskHandle() const`
  - Returns FreeRTOS task handle for `_rxTask`. Used by transports (e.g. TinyUSB callback `tud_vendor_rx_cb`) to wake `_rxTask` via `xTaskNotifyGive()`.
- `RingbufHandle_t getCmdRingBuffer() const`
  - Diagnostic getter returning internal command ringbuffer handle.
- `size_t getAccumulatorCursor() const`
  - Diagnostic getter returning current accumulator cursor position.
- `size_t getAccumulatorExpectedLen() const`
  - Diagnostic getter returning expected payload length of packet under assembly.
- `size_t processRawBuffer(const uint8_t *data, size_t len)`
  - Ingests arbitrary chunk sizes (1-byte or multi-byte) from USB/UART transport.
  - Reassembles fragmented iAP frames, handles multi-`0xFF` sync headers, validates length and two's complement checksums.
  - Automatically enqueues verified complete frames to `_cmdRingBuffer`.
  - `@param[in] data Pointer to raw iAP byte buffer.`
  - `@param[in] len Length of data in bytes.`
  - `@return size_t Number of bytes successfully processed.`

### Metadata & State Update Methods
- `void play(bool noLoop = false)`
  - Updates playback state to PLAYING and notifies host if subscribed.
- `void pause(bool noLoop = false)`
  - Updates playback state to PAUSED and notifies host if subscribed.
- `void stop(bool noLoop = false)`
  - Updates playback state to STOPPED and notifies host if subscribed.
- `void updatePlayPosition(uint32_t position)`
  - `@param[in] position Play position in milliseconds.`
- `void updateAlbumName(const char *incAlbumName)`
  - `@param[in] incAlbumName Null-terminated album title string.`
- `void updateArtistName(const char *incArtistName)`
  - `@param[in] incArtistName Null-terminated artist name string.`
- `void updateTrackTitle(const char *incTrackTitle)`
  - `@param[in] incTrackTitle Null-terminated track title string.`
- `void updateTrackDuration(uint32_t incTrackDuration)`
  - `@param[in] incTrackDuration Track duration in milliseconds.`

## Related Links
- [Component TOO](TOO.md)
- [Component README](../README.md)
