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
  - Resets internal state machine variables, clears track indices, cancels pending timers.
- `void attachPlayControlHandler(playStatusHandler_t playHandler)`
  - `@param[in] playHandler Pointer to playback controller callback function.`
- `void attachTxHandler(rawTxHandler_t txHandler)`
  - `@param[in] txHandler Pointer to raw transport transmit callback function.`
- `size_t processRawBuffer(const uint8_t *data, size_t len)`
  - `@param[in] data Pointer to raw iAP byte buffer.`
  - `@param[in] len Length of data in bytes.`
  - `@return size_t Number of bytes successfully pushed to ringbuffer.`

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
