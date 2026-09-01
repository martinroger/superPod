# superPod Wireless Audio & Control: Functional & Behavioral Requirements Specification

This document provides a comprehensive, implementation-agnostic specification of all functional and behavioral requirements for the `superPod` wireless audio and media control subsystem. It serves as the baseline and verification criteria for migrating from Classic Bluetooth (A2DP / AVRCP) to BLE Audio (BAP / MCP / MCS).

---

## 1. System Architecture Overview

```
                      ┌────────────────────────────────────────────────────────┐
                      │                   Wireless Audio Host                  │
                      │               (Smartphone / Audio Source)              │
                      └───────────▲────────────────────────────────┬───────────┘
                                  │ Control Commands               │ Audio Stream
                                  │ (Play/Pause/Next/Prev)         │ Metadata & Position
                                  │                                │ 
┌─────────────────────────────────┴────────────────────────────────┴───────────────────────────────────┐
│ superPod Wireless Audio Subsystem (Core 0 / PRO_CPU)                                                 │
│                                                                                                      │
│  [REQ-SEC]   Security & Pairing       : "Just Works", No PIN, Auto-Confirm, Bond Storage              │
│  [REQ-CONN]  Connection Lifecycle     : Configurable Name, 10s Auto-Reconnect, Auto-Play on Connect  │
│  [REQ-AUD]   Audio Streaming & I2S    : Uncompressed PCM 44.1/48kHz, Glitch-Free DMA, Loopback Inh.  │
│  [REQ-CTRL]  Playback Control Bridge  : Bi-directional Commands, Pending Track-Change ACK Gate       │
│  [REQ-META]  Track Metadata Ingestion : Title/Artist/Album/Duration parsing, 1s Play Position Cadence│
│  [REQ-SYS]   Task & Core Affinities   : Core 0 Isolation, Non-blocking Asynchronous Queues           │
└─────────────────────────────────┬────────────────────────────────────────────────────────────────────┘
                                  │ In-Memory Cross-Core Bridge
                                  ▼
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ superPod iPod Emulation Engine & USB Bridge (Core 1 / APP_CPU)                                       │
│                                                                                                      │
│  - esPod Lingo Engine (L0x00 General, L0x03 Simple Remote, L0x04 Extended Interface)                │
│  - TinyUSB PL2303 CDC/Vendor Bridge                                                                  │
└──────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Detailed Functional & Behavioral Requirements

### Section 1: Security, Pairing & Bond Management (`REQ-SEC`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-SEC-1** | **No-PIN ("Just Works") Pairing** | The peripheral must NOT require entering a static PIN code (e.g. `0000`) or keyboard passkey on the client device. IO capabilities must be set to `NoInputNoOutput` (`ESP_BT_IO_CAP_NONE` / `ESP_BLE_IO_CAP_NONE`). |
| **REQ-SEC-2** | **Auto-Confirm Pairing Requests** | All incoming Simple Secure Pairing (SSP) / BLE Security Manager (SMP) numerical confirmation requests must be automatically confirmed (`confirm_reply(true)`) without requiring user interaction. |
| **REQ-SEC-3** | **Encryption & Bond Persistence** | The system must negotiate authenticated/unauthenticated encryption with bonding. Long Term Keys (LTK) and peer identity (BDA/IRK) must be saved into non-volatile storage (NVS) to permit seamless reconnection. |

---

### Section 2: Connection Lifecycle & Auto-Reconnect (`REQ-CONN`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-CONN-1** | **Configurable Device Name** | The device must broadcast an identifiable, configurable friendly name (e.g., `CONFIG_A2DP_SINK_NAME` / `CONFIG_BLE_AUDIO_SINK_NAME`, default: `superPod-A2DP`). |
| **REQ-CONN-2** | **Continuous Auto-Reconnect** | Upon link loss (out of range, peer Bluetooth toggle, or MCU reboot), the system must read the last bonded BDA from NVS and automatically attempt reconnection every 10,000 ms until restored. Auto-reconnection must only cease upon an explicit user disconnect. |
| **REQ-CONN-3** | **Connect Lifecycle Hooking** | Upon successful connection (`CONNECTED` state):<br>1. Mark the iPod engine active: `espod.disabled = false`.<br>2. Automatically issue a `play()` request to the peer device to resume audio playback immediately. |
| **REQ-CONN-4** | **Disconnect Lifecycle Hooking** | Upon loss of link (`DISCONNECTED` state):<br>1. Disable the iPod engine: `espod.disabled = true`.<br>2. Reset protocol state and clear track/metadata caches: `espod.resetState()`. |

---

### Section 3: Audio Streaming & Digital Output (`REQ-AUD`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-AUD-1** | **Audio Sink Role** | The MCU must operate as an audio sink (A2DP Sink / BAP Unicast Server), supporting stereo audio at minimum 44.1 kHz or 48.0 kHz sample rate, 16-bit depth. |
| **REQ-AUD-2** | **Continuous I2S DMA Streaming** | Decoded audio samples must be delivered directly to the external DAC via ESP-IDF I2S master driver (`CONFIG_I2S_BCLK_PIN`, `CONFIG_I2S_WS_PIN`, `CONFIG_I2S_DOUT_PIN`) over DMA with zero audible dropouts or buffer underruns during USB activity. |
| **REQ-AUD-3** | **Audio State Sync & Loopback Prevention** | Remote audio state transitions must update the `espod` state engine while strictly preventing command echo back to the phone:<br>- **Stream Started**: Call `espod.play(true)` (`noLoop = true`).<br>- **Stream Suspended/Paused**: Call `espod.pause(true)` (`noLoop = true`). |

---

### Section 4: Bi-directional Playback Control Bridge (`REQ-CTRL`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-CTRL-1** | **iAP Command Mapping** | Inbound playback commands from the dock/car head-unit (`playStatusHandler`) must map directly to wireless host commands:<br>- `PB_CMD_PLAY` $\longrightarrow$ Send Host Play<br>- `PB_CMD_PAUSE` $\longrightarrow$ Send Host Pause<br>- `PB_CMD_STOP` $\longrightarrow$ Send Host Stop<br>- `PB_CMD_NEXT` / `PB_CMD_NEXT_TRACK` $\longrightarrow$ Send Host Next Track<br>- `PB_CMD_PREV` / `PB_CMD_PREVIOUS_TRACK` $\longrightarrow$ Send Host Previous Track |
| **REQ-CTRL-2** | **Pending Track-Change ACK Gate** | When a Next or Previous Track command is received from the car dock, `espod` records `trackChangeAckPending = cmdID` and holds the iAP ACK packet. The wireless subsystem MUST deliver updated track metadata upon track transition; only then does `espod._checkAllMetaUpdated()` release `L0x00::_0x02_iPodAck(iPodAck_OK)`. Failure to provide new metadata will cause the dock's command ACK to stall. |
| **REQ-CTRL-3** | **Remote Player Command Reception** | If the user triggers Play, Pause, or Skip directly on the phone screen, the wireless subsystem must notify the MCU, updating `espod` playback status accordingly. |

---

### Section 5: Track Metadata & Position Synchronization (`REQ-META`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-META-1** | **Metadata Callback Hook** | The wireless stack must provide a callback hook receiving attribute IDs and string pointers for:<br>- **Title** $\longrightarrow$ `espod.updateTrackTitle(char*)`<br>- **Artist** $\longrightarrow$ `espod.updateArtistName(char*)`<br>- **Album** $\longrightarrow$ `espod.updateAlbumName(char*)`<br>- **Playing Time / Duration** $\longrightarrow$ Converted from ASCII ms to `uint32_t` $\longrightarrow$ `espod.updateTrackDuration(uint32_t)` |
| **REQ-META-2** | **Attribute Filtering & Empty String Protection** | Metadata callbacks must validate payloads:<br>1. Reject `NULL` pointers.<br>2. For text fields (Title, Artist, Album), reject empty strings (`""`) to prevent wiping valid dock display text with blank intermediate frames. |
| **REQ-META-3** | **Asynchronous Queue Transfer** | Metadata strings must be duplicated (`strdup`) and dispatched across an asynchronous FreeRTOS queue (`avrcMetadataQueue` / `mediaMetadataQueue`) of depth $\ge 32$. If the queue is full, the allocated memory must be freed immediately to prevent memory leaks. |
| **REQ-META-4** | **1-Second Play Position Notification** | The wireless stack must register periodic playback position reporting with the host at a 1-second cadence. Incoming position events in milliseconds must trigger `espod.updatePlayPosition(play_pos_ms)`. |
| **REQ-META-5** | **Downstream Position Push to Dock** | When `espod.updatePlayPosition` updates, if the dock has subscribed to play status notifications (`playStatusNotificationState == NOTIF_ON`), `espod` must dispatch L0x04 `_0x27_PlayStatusNotification(0x04, playPosition)` over USB to advance the progress bar. |

---

### Section 6: System Concurrency & Fault Tolerance (`REQ-SYS`)

| ID | Requirement Name | Description & Behavioral Rule |
| :--- | :--- | :--- |
| **REQ-SYS-1** | **Core Affinity Isolation (Option B Swapped)** | - **Core 0 (PRO_CPU)**: Radio Controller, Wireless Host Stack, Audio Decoding & I2S DMA, Metadata Queue Processing Task (Priority 6, Stack $\ge 4096$ B).<br>- **Core 1 (APP_CPU)**: TinyUSB Device Stack, USB $\leftrightarrow$ `espod` Bridge Task, `espod` Lingo State Machine. |
| **REQ-SYS-2** | **Memory & Deduplication Safety** | `espod` must compare incoming strings (`strcmp`) and update flags only on actual changes. Queue processing tasks must free string payloads after consumption. |
| **REQ-SYS-3** | **Rapid Command Robustness** | Rapid successive skip commands from either dock or phone must not cause task starvation, queue overflow, or heap exhaustion. |

---

## 3. Implementation Mapping: A2DP/AVRCP vs. BLE Audio

This mapping table defines how each functional requirement is fulfilled in the current Classic BT stack and how it translates to BLE Audio:

| Requirement Area | Classic Bluetooth (Current Stack) | BLE Audio / LE Audio (Target Stack) |
| :--- | :--- | :--- |
| **No-PIN Pairing** | Classic SSP `ESP_BT_IO_CAP_NONE`, PIN fixed 0 | BLE SMP `ESP_BLE_IO_CAP_NONE`, "Just Works" |
| **Auto-Reconnect** | `BluetoothA2DPCommon::reconnect()` + NVS BDA blob | BLE GAP Directed Advertising / `esp_ble_gap_connect` |
| **Connect Lifecycle** | `ESP_A2D_CONNECTION_STATE_CONNECTED` callback | BLE ACL / CIS `ESP_BLE_AUDIO_GAP_EVENT_ACL_CONNECT` |
| **Audio Stream Sink** | A2DP Sink (SBC/AAC decoder) | BAP Unicast Server (ASE Sink, LC3 decoder) |
| **Audio Output** | `I2SStream` / `esp_driver_i2s` DMA | `esp_driver_i2s` DMA (identical GPIOs) |
| **Loopback Prevention** | `espod.play(true)` / `pause(true)` (`noLoop = true`) | `espod.play(true)` / `pause(true)` (`noLoop = true`) |
| **Control Bridge** | AVRCP Passthrough (Play/Pause/Next/Prev) | MCP (Media Control Profile) Control Point OpCodes |
| **ACK Gate Release** | `esp_avrc_ct_cb` metadata event $\to$ `_checkAllMetaUpdated()` | MCS GATT characteristic notification $\to$ `_checkAllMetaUpdated()` |
| **Track Metadata** | AVRCP `ESP_AVRC_MD_ATTR_*` $\to$ `avrc_metadata_callback` | MCS Track Title, Duration, OTS characteristics |
| **Play Position Sync** | AVRCP RN Play Position Callback (1s interval) | MCS Track Position GATT notification (1s interval) |
| **Task Allocation** | Core 0 (A2DP + AVRC task), Core 1 (USB + espod) | Core 0 (BAP + MCP task), Core 1 (USB + espod) |

---

## 4. Verification & Acceptance Criteria Matrix

| Test Case ID | Test Item | Verification Procedure | Expected Outcome |
| :--- | :--- | :--- | :--- |
| **TC-SEC-01** | No-PIN Pairing | Initiate pairing from a new iPhone or Android device. | Device pairs seamlessly with "Just Works"; no PIN or passkey prompt appears. |
| **TC-CONN-01** | Auto-Reconnect | 1. Pair device.<br>2. Walk out of range / disable phone Bluetooth.<br>3. Restore connection after 30s. | Device reconnects automatically within 10s without requiring MCU reboot. |
| **TC-CONN-02** | Connect Auto-Play | Connect phone to superPod. | Serial log prints `espod enabled`; phone automatically starts media playback. |
| **TC-AUD-01** | Glitch-Free Audio | Stream audio continuously for 30 minutes while sending active iAP dock commands. | Continuous high-fidelity audio via I2S DAC; zero buffer underruns, clicks, or pops. |
| **TC-CTRL-01** | Dock Control Responsiveness | Press Play, Pause, Next, Previous on dock/steering wheel. | Phone player reacts within $<150$ ms; correct track starts playing. |
| **TC-CTRL-02** | Loopback Prevention | Press Pause on phone screen. | `espod` updates to Pause state without sending a redundant Pause command back to the phone. |
| **TC-CTRL-03** | Track Change ACK Gate | Press Next Track on dock. | Dock receives `iPodAck_OK` immediately after new track metadata is delivered; no timeout. |
| **TC-META-01** | Metadata Accuracy | Change tracks on phone media player. | Dock display displays exact Track Title, Artist, Album, and Total Duration. |
| **TC-META-02** | Play Position Cadence | Observe progress bar on car dock while track is playing. | Progress bar increments smoothly every 1 second. |
| **TC-META-03** | Rapid Skip Stress Test | Skip 10 tracks rapidly on dock or phone. | No heap leakage, no FreeRTOS task starvation, metadata displays latest track correctly. |

