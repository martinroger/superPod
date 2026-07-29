/**
 * @file esPod.h
 * @brief Master Header for the esPod (Apple iPod Accessory Protocol) Library Component.
 * 
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * The esPod class provides an Apple iPod Accessory Protocol (iAP / AAP) state machine,
 * supporting General Lingo (0x00), Simple Remote Lingo (0x03), and Extended Interface
 * Lingo (0x04).
 * 
 * Hybridization & Transport Architecture:
 *   - Arduino Mode: Operates as an Arduino library communicating over HardwareSerial/Stream.
 *   - Native ESP-IDF Component Mode: Operates as a native ESP-IDF C++ component.
 *   - Direct Raw iAP Ingestion API: Adds processRawBuffer() and writeRawStream() methods.
 *     In single-MCU mode, raw iAP message packets received from TinyUSB PL2303 vendor Bulk OUT
 *     endpoints are pushed directly into _cmdRingBuffer in RAM, completely bypassing physical
 *     UART hardware baudrate limits while preserving full iAP frame verification.
 * =================================================================================
 */

#pragma once

#include <cstdint>
#include <climits>
#include "L0x00.h"
#include "L0x03.h"
#include "L0x04.h"
#include "esPod_conf.h"
#include "esPod_utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "driver/uart.h"

class esPod
{
    friend class L0x00;
    friend class L0x03;
    friend class L0x04;

public:
    /// @brief External callback function type for controlling audio source playback FROM the esPod object
    typedef void playStatusHandler_t(PB_COMMAND playControlCommand);

    // State variables
    bool extendedInterfaceModeActive = false; // Indicates extended interface mode is active (Lingo 0x04)
    bool disabled = true;                     // Disables parsing while starting up or disconnected

    // Metadata variables
    char trackTitle[255] = "Title";  // Current track Title
    char prevTrackTitle[255] = " ";  // Previous track Title
    char artistName[255] = "Artist"; // Current track Artist Name
    char prevArtistName[255] = " ";  // Previous track Artist Name
    char albumName[255] = "Album";   // Current track Album Name
    char prevAlbumName[255] = " ";   // Previous track Album Name
    char trackGenre[255] = "Genre";  // Current track Genre
    char playList[255] = "Spotify";  // Current playlist name
    char composer[255] = "Composer"; // Current track Composer
    uint32_t trackDuration = 1;      // Track duration in ms
    uint32_t prevTrackDuration = 0;  // Previous track duration in ms
    uint32_t playPosition = 0;       // Current play position in ms

    // Playback Engine state
    uint8_t playStatus = PB_STATE_PAUSED;            // Current state of Playback Engine
    uint8_t playStatusNotificationState = NOTIF_OFF; // Notification engine state
    uint8_t trackChangeAckPending = 0x00;            // Pending track change marker
    uint64_t trackChangeTimestamp = 0;            // Timestamp for last track change request
    uint8_t shuffleStatus = 0x00;                    // 0x00: No Shuffle, 0x01: Tracks, 0x02: Albums
    uint8_t repeatStatus = 0x02;                     // 0x00: Off, 0x01: One, 0x02: All

    // TrackList state
    uint32_t currentTrackIndex = 0;                      // Internal current track index
    uint32_t prevTrackIndex = TOTAL_NUM_TRACKS - 1;      // Internal previous track index
    const uint32_t totalNumberTracks = TOTAL_NUM_TRACKS; // Total track list size
    uint32_t trackList[TOTAL_NUM_TRACKS] = {0};          // Track list indices
    uint32_t trackListPosition = 0;                      // Track list cursor position

    /**
     * @brief Master constructor for esPod class.
     * 
     * @param uartNum Hardware UART port number (1 for UART_NUM_1).
     * @param rxPin RX pin number (-1 for default/unassigned).
     * @param txPin TX pin number (-1 for default/unassigned).
     * @param baud Baudrate (19200 default for Apple AAP).
     */
    esPod(uint8_t uartNum = 1, int rxPin = -1, int txPin = -1, uint32_t baud = 19200);

    /**
     * @brief Destructor for esPod class.
     */
    ~esPod();

    /**
     * @brief Resets esPod instance state to clean startup defaults.
     */
    void resetState();

    /**
     * @brief Attaches external play control callback function to synchronize esPod commands with audio source.
     * 
     * @param playHandler Pointer to playStatusHandler_t callback function.
     */
    void attachPlayControlHandler(playStatusHandler_t playHandler);

    /**
     * @brief Direct Raw iAP Message Processing API.
     * 
     * Ingestion entry point for receiving raw Apple Accessory Protocol packets directly from
     * memory buffers (e.g., TinyUSB PL2303 Bulk OUT endpoint) in single-MCU mode.
     * 
     * @param data Pointer to raw iAP byte buffer.
     * @param len Length of data in bytes.
     * @return size_t Number of bytes successfully pushed to processing queue.
     */
    size_t processRawBuffer(const uint8_t *data, size_t len);

    /**
     * @brief Updates playback engine state to PLAYING.
     * 
     * @param noLoop If true, updates internal state only without triggering playStatusHandler callback.
     */
    void play(bool noLoop = false);

    /**
     * @brief Updates playback engine state to PAUSED.
     * 
     * @param noLoop If true, updates internal state only without triggering playStatusHandler callback.
     */
    void pause(bool noLoop = false);

    /**
     * @brief Updates playback engine state to STOPPED.
     * 
     * @param noLoop If true, updates internal state only without triggering playStatusHandler callback.
     */
    void stop(bool noLoop = false);

    /**
     * @brief Updates current playback position in milliseconds.
     * 
     * @param position Play position in ms.
     */
    void updatePlayPosition(uint32_t position);

    /**
     * @brief Updates current album name string.
     * 
     * @param incAlbumName Null-terminated album name string.
     */
    void updateAlbumName(const char *incAlbumName);

    /**
     * @brief Updates current artist name string.
     * 
     * @param incArtistName Null-terminated artist name string.
     */
    void updateArtistName(const char *incArtistName);

    /**
     * @brief Updates current track title string.
     * 
     * @param incTrackTitle Null-terminated track title string.
     */
    void updateTrackTitle(const char *incTrackTitle);

    /**
     * @brief Updates current track duration in milliseconds.
     * 
     * @param incTrackDuration Duration in ms.
     */
    void updateTrackDuration(uint32_t incTrackDuration);

private:
    // UART parameters
    uart_port_t _uartPort;
    int _rxPin;
    int _txPin;
    uint32_t _baudrate;
    bool _isBaudReady = false;
    QueueHandle_t _uartEventQueue;

    // Track metadata update flags
    bool _albumNameUpdated = false;
    bool _artistNameUpdated = false;
    bool _trackTitleUpdated = false;
    bool _trackDurationUpdated = false;
    void _checkAllMetaUpdated();

    // FreeRTOS Queues and RingBuffer
    RingbufHandle_t _cmdRingBuffer;
    QueueHandle_t _txFreeBufferQueue;
    uint8_t _txBufferPool[TX_QUEUE_SIZE][MAX_PACKET_SIZE];
    QueueHandle_t _txQueue;
    QueueHandle_t _timerQueue;

    // FreeRTOS tasks
    TaskHandle_t _rxTaskHandle;
    TaskHandle_t _processTaskHandle;
    TaskHandle_t _txTaskHandle;
    TaskHandle_t _timerTaskHandle;

    esp_err_t _initFreeRTOSStack();

    static void _rxTask(void *pvParameters);
    static void _processTask(void *pvParameters);
    static void _txTask(void *pvParameters);
    static void _timerTask(void *pvParameters);

    // FreeRTOS software timers
    TimerHandle_t _pendingTimer_0x00;
    TimerHandle_t _pendingTimer_0x03;
    TimerHandle_t _pendingTimer_0x04;

    static void _pendingTimerCallback_0x00(TimerHandle_t xTimer);
    static void _pendingTimerCallback_0x03(TimerHandle_t xTimer);
    static void _pendingTimerCallback_0x04(TimerHandle_t xTimer);

    uint8_t _pendingCmdId_0x00;
    uint8_t _pendingCmdId_0x03;
    uint8_t _pendingCmdId_0x04;

    static uint8_t _checksum(const uint8_t *byteArray, uint32_t len);
    void _sendPacket(const uint8_t *byteArray, uint32_t len);
    void _queuePacket(const uint8_t *byteArray, uint32_t len);
    void _queuePacketToFront(const uint8_t *byteArray, uint32_t len);
    void _processPacket(const uint8_t *byteArray, size_t len);

    bool _rxIncomplete = false;

    // Device metadata
    const char *_name = ESPIPOD_NAME;
    const uint8_t _SWMajor = 0x01;
    const uint8_t _SWMinor = 0x03;
    const uint8_t _SWrevision = 0x00;
    const char *_serialNumber = "AB345F7HIJK";

    playStatusHandler_t *_playStatusHandler = nullptr;
};
