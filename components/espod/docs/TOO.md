# `espod` Theory of Operation (TOO)

## Architecture Overview

The `esPod` class operates as an event-driven iAP protocol state machine executing on FreeRTOS tasks pinned to Core 1.

```mermaid
graph TD
    USB[USB Bulk OUT / UART Stream] -->|Notify / Interrupt| RxTask[esPod _rxTask]
    RxTask -->|_rawRxHandler / UART Read| Acc[esPod Stream Accumulator]
    Acc -->|Preamble Sync 0xFF 0x55| AccState{Rx Incomplete?}
    AccState -->|Len & Checksum Verified| RingBuf[_cmdRingBuffer]
    AccState -->|Timeout / Corrupt / Bad Len| Discard[Discard & Reset Accumulator]
    RingBuf -->|_processTask| Parser[_processPacket]
    Parser --> L0x00[L0x00 General Lingo]
    Parser --> L0x03[L0x03 Simple Remote]
    Parser --> L0x04[L0x04 Extended Interface]
    L0x04 -->|_sendPacket| TxQueue[_txQueue]
    TxQueue -->|_txTask| TxHandler[_rawTxHandler / usb_tx_handler]
    TxHandler --> USBIN[TinyUSB Bulk IN]
```

### 1. Ingestion Pipeline & Universal `_rxTask`
Raw iAP byte streams from physical UART or USB Bulk OUT endpoints enter `esPod` via its universal `_rxTask`.
- When operating over USB, an external transport callback is registered via `attachRxHandler(pl2303_usb_read_bytes)`.
- When incoming USB Bulk OUT packets arrive, `tud_vendor_rx_cb()` fires `xTaskNotifyGive()` to the handle returned by `getRxTaskHandle()`.
- `_rxTask` executes an event-driven loop with dynamic `ulTaskNotifyTake` wait ticks:
  - **Mid-packet inter-byte timeout (500 ms)**: Enforced when `_rxIncomplete == true`. If the stream stalls for $\ge 500\text{ ms}$, `resetAccumulator()` is called and partial data is discarded.
  - **Serial idle timeout (8000 ms) with debounce latch**: Enforced when idle. If 8000 ms elapses without data, `resetState()` is invoked and `serialTimedOut` latches `true` (transitioning wait to `portMAX_DELAY` to eliminate repeated reset thrashing).
- Read bytes enter `processRawBuffer()`, where the internal accumulator hunts for `0xFF 0x55` (absorbing multi-`0xFF` padding), extracts length, accumulates payload, and verifies two's complement checksum. Complete packets are safely pushed to `_cmdRingBuffer`.

```mermaid
flowchart TD
    DataIn[Raw Bytes Ingested] --> Loop{"More Bytes in Buffer?"}
    Loop -- "Yes" --> StCheck{"_rxIncomplete == true?"}
    
    %% Hunt Mode
    StCheck -- "No (Hunt Mode)" --> Preamble{"prevByte == 0xFF<br/>AND byte == 0x55?"}
    Preamble -- "Yes (Matched)" --> EnterCapture["_rxIncomplete = true<br/>_accCursor = 2"]
    Preamble -- "No" --> NextB["_accPrevByte = byte"]
    EnterCapture --> NextB --> Loop
    
    %% Capture Mode
    StCheck -- "Yes (Capture Mode)" --> LenCheck{"_accCursor == 2?<br/>(Length Byte)"}
    LenCheck -- "Yes" --> ValLen{"1 <= len <= MAX-4?"}
    ValLen -- "No (Invalid)" --> Reset1["resetAccumulator()"] --> NextB
    ValLen -- "Valid" --> StoreL["_accExpectedLen = byte<br/>_accBuffer[_accCursor++] = byte"] --> NextB
    
    LenCheck -- "No" --> AppB["_accBuffer[_accCursor++] = byte"]
    AppB --> DoneCheck{"_accCursor ==<br/>3 + expectedLen + 1?"}
    DoneCheck -- "No (Incomplete)" --> NextB
    DoneCheck -- "Yes (Complete)" --> ChkMatch{"_checksum() == rxChecksum?"}
    ChkMatch -- "Valid" --> PushRing["xRingbufferSend(_cmdRingBuffer)"] --> Reset2["resetAccumulator()"] --> NextB
    ChkMatch -- "Invalid" --> Reset2
    
    %% Buffer Exhausted Loop-Back
    Loop -- "No (Buffer Empty)" --> CheckRx{"_rxIncomplete == true?"}
    CheckRx -- "Yes (Mid-Packet)" --> WaitMid["Wait for next burst<br/>(up to 500ms timeout)"]
    WaitMid -- "LOOP-BACK: Next burst arrives < 500ms" --> DataIn
    WaitMid -- "TIMEOUT: >= 500ms elapsed" --> TimeoutReset["resetAccumulator()"]
    
    CheckRx -- "No (Idle)" --> WaitIdle["Wait for next packet<br/>(up to 8000ms idle timeout)"]
    WaitIdle -- "LOOP-BACK: New traffic arrives" --> DataIn
```

### 2. Processing Pipeline (`_processTask`)
- `_processTask` blocks on `xRingbufferReceive(..., portMAX_DELAY)`.
- It verifies preamble `0xFF 0x55` and frame checksum before passing command payload to the target Lingo handler (`L0x00::processLingo`, `L0x03::processLingo`, `L0x04::processLingo`).

### 3. Outbound Transport Pipeline (`_txTask`)
- Lingo handlers construct response packets wrapped in `0xFF 0x55 [len] [payload] [checksum]` and push them to `_txQueue`.
- `_txTask` blocks on `xQueueReceive(_txQueue, ..., portMAX_DELAY)`.
- When dequeued, if `_rawTxHandler` is attached (via `attachTxHandler`), `_txTask` invokes the handler (`usb_tx_handler` -> `pl2303_usb_write_bytes`).

### 4. Software Timers & Pending ACKs (`_timerTask`)
- Commands requiring delayed acknowledgment (e.g. track change requests waiting on Bluetooth AVRCP metadata) start a software timer (`_pendingTimer_0x04`).
- Upon expiration, `_pendingTimerCallback` enqueues a message to `_timerQueue`, waken `_timerTask` to auto-fire `iPodAck_OK` to the accessory host.

## Related Links
- [Component API Reference](API.md)
- [Component README](../README.md)
- [System Theory of Operation](../../../TOO.md)
