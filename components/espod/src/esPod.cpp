/**
 * @file esPod.cpp
 * @brief Master Implementation for the esPod (Apple iPod Accessory Protocol) Library Component.
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

#include "esPod.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdlib>
#include <cstring>

static const char *TAG = "esPod";

#pragma region Constructor, Destructor & State Management

esPod::esPod(uint8_t uartNum, int rxPin, int txPin, uint32_t baud)
    : _uartPort((uart_port_t)uartNum), _rxPin(rxPin), _txPin(txPin), _baudrate(baud)
{
    if (uartNum > UART_NUM_MAX)
    {
        ESP_LOGE(TAG, "Invalid UART port number, defaulting to UART port 1");
        _uartPort = UART_NUM_1;
    }

    if (_rxPin >= 0 && _txPin >= 0)
    {
        if (uart_is_driver_installed(_uartPort))
        {
            uart_driver_delete(_uartPort);
        }

        uart_config_t uart_config = {
            .baud_rate = (int)_baudrate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        esp_err_t uartRet = uart_driver_install(_uartPort, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 20, &_uartEventQueue, 0);
        if (uartRet == ESP_OK)
        {
            uart_param_config(_uartPort, &uart_config);
            uart_set_pin(_uartPort, _txPin, _rxPin, -1, -1);
        }
    }

    _isBaudReady = true;

    if (_initFreeRTOSStack() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize esPod FreeRTOS stack");
        return;
    }
}

esPod::~esPod()
{
    aapCommand tempCmd;
    if (_rxTaskHandle) vTaskDelete(_rxTaskHandle);
    if (_processTaskHandle) vTaskDelete(_processTaskHandle);
    if (_txTaskHandle) vTaskDelete(_txTaskHandle);
    if (_timerTaskHandle) vTaskDelete(_timerTaskHandle);

    stopTimer(_pendingTimer_0x00);
    stopTimer(_pendingTimer_0x03);
    stopTimer(_pendingTimer_0x04);
    if (_pendingTimer_0x00) xTimerDelete(_pendingTimer_0x00, 0);
    if (_pendingTimer_0x03) xTimerDelete(_pendingTimer_0x03, 0);
    if (_pendingTimer_0x04) xTimerDelete(_pendingTimer_0x04, 0);

    if (_cmdRingBuffer != NULL)
        vRingbufferDelete(_cmdRingBuffer);

    while (xQueueReceive(_txQueue, &tempCmd, 0) == pdTRUE)
    {
        if (tempCmd.payload)
        {
            delete[] tempCmd.payload;
            tempCmd.payload = nullptr;
        }
    }
}

#pragma endregion

#pragma region Direct Raw Ingestion API

/// @brief Direct In-Memory Raw iAP Packet Stream Accumulator (processes arbitrary chunk sizes: 1-byte or multi-byte)
size_t esPod::processRawBuffer(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0 || _cmdRingBuffer == nullptr)
        return 0;

    size_t processedBytes = 0;

    for (size_t i = 0; i < len; i++)
    {
        uint8_t incByte = data[i];
        processedBytes++;

        // State 1: Hunting for preamble (0xFF 0x55)
        if (!_rxIncomplete)
        {
            if (_accPrevByte == 0xFF && incByte == 0x55)
            {
                _rxIncomplete = true;
                _accCursor = 0;
                _accExpectedLen = 0;
                _accBuffer[0] = 0xFF;
                _accBuffer[1] = 0x55;
                _accCursor = 2; // Position for length byte
                ESP_LOGD(TAG, "Accumulator synced on preamble 0xFF 0x55");
            }
            // Retain consecutive 0xFFs (handles 0xFF 0xFF 0x55 sequences cleanly)
            _accPrevByte = incByte;
        }
        // State 2: Mid-packet capture
        else
        {
            // Expected length byte is at cursor position 2
            if (_accCursor == 2)
            {
                _accExpectedLen = incByte;
                if (_accExpectedLen == 0 || _accExpectedLen > (MAX_PACKET_SIZE - 4))
                {
                    ESP_LOGW(TAG, "Accumulator invalid length byte: %lu, discarding", (unsigned long)_accExpectedLen);
                    resetAccumulator();
                    _accPrevByte = incByte;
                    continue;
                }
                _accBuffer[_accCursor++] = incByte;
            }
            else
            {
                _accBuffer[_accCursor++] = incByte;

                // Check if we have received the full frame: 2 (preamble) + 1 (len) + payloadLen + 1 (checksum)
                if (_accCursor == (size_t)(3 + _accExpectedLen + 1))
                {
                    _rxIncomplete = false;

                    // Verify checksum: _checksum computes 0x100 - sum(length + payload)
                    uint8_t calcChecksum = _checksum(&_accBuffer[3], _accExpectedLen);
                    uint8_t rxChecksum = incByte;

                    if (calcChecksum == rxChecksum)
                    {
                        // Verified complete packet -> push entire frame into _cmdRingBuffer
                        BaseType_t ret = xRingbufferSend(_cmdRingBuffer, (void *)_accBuffer, _accCursor, pdMS_TO_TICKS(10));
                        if (ret != pdTRUE)
                        {
                            ESP_LOGW(TAG, "cmdRingBuffer full, dropping %u byte packet", (unsigned int)_accCursor);
                        }
                        else
                        {
                            ESP_LOGD(TAG, "Assembled iAP frame (%u bytes) queued to ringbuffer", (unsigned int)_accCursor);
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Accumulator checksum error: calc 0x%02X vs rx 0x%02X, discarding", calcChecksum, rxChecksum);
                    }

                    resetAccumulator();
                }
            }
            _accPrevByte = incByte;
        }
    }

    return processedBytes;
}

void esPod::resetState()
{
    stopTimer(_pendingTimer_0x00);
    stopTimer(_pendingTimer_0x03);
    stopTimer(_pendingTimer_0x04);
    _pendingCmdId_0x00 = 0x00;
    _pendingCmdId_0x03 = 0x00;
    _pendingCmdId_0x04 = 0x00;

    extendedInterfaceModeActive = false;
    playStatusNotificationState = NOTIF_OFF;
    playStatus = PB_STATE_PAUSED;
    playPosition = 0;
    trackChangeAckPending = 0x00;
    _albumNameUpdated = false;
    _artistNameUpdated = false;
    _trackTitleUpdated = false;
    _trackDurationUpdated = false;

    currentTrackIndex = 0;
    prevTrackIndex = TOTAL_NUM_TRACKS - 1;
    for (uint16_t i = 0; i < TOTAL_NUM_TRACKS; i++)
    {
        trackList[i] = 0;
    }
    trackListPosition = 0;

    // Reset stream accumulator
    resetAccumulator();

    // Flush any pending TX queue items and return buffers to free pool
    aapCommand tempCmd;
    while (_txQueue && xQueueReceive(_txQueue, &tempCmd, 0) == pdTRUE)
    {
        if (tempCmd.payload != nullptr)
        {
            uint8_t *bufPtr = tempCmd.payload;
            xQueueSend(_txFreeBufferQueue, &bufPtr, 0);
        }
    }

    // Reset ring buffer
    if (_cmdRingBuffer)
    {
        size_t tempSize = 0;
        void *tempItem = nullptr;
        while ((tempItem = xRingbufferReceive(_cmdRingBuffer, &tempSize, 0)) != NULL)
        {
            vRingbufferReturnItem(_cmdRingBuffer, tempItem);
        }
    }

    // Flush any pending timer callback messages
    TimerCallbackMessage tempTimerMsg;
    while (_timerQueue && xQueueReceive(_timerQueue, &tempTimerMsg, 0) == pdTRUE)
    {
        // Discard pending timer message
    }

    ESP_LOGI(TAG, "State reset clean");
}

void esPod::attachPlayControlHandler(playStatusHandler_t playHandler)
{
    // Save pointer to the user-supplied playback control handler
    _playStatusHandler = playHandler;
    ESP_LOGI(TAG, "playStatusHandler attached");
}

void esPod::attachTxHandler(rawTxHandler_t txHandler)
{
    // Save pointer to external raw transport transmit function (e.g., TinyUSB PL2303 write)
    _rawTxHandler = txHandler;
    ESP_LOGI(TAG, "rawTxHandler attached");
}

void esPod::attachRxHandler(rawRxHandler_t rxHandler)
{
    // Save pointer to external raw transport receive function
    _rawRxHandler = rxHandler;
    ESP_LOGI(TAG, "rawRxHandler attached");
}

void esPod::resetAccumulator()
{
    _accCursor = 0;
    _accExpectedLen = 0;
    _accPrevByte = 0x00;
    _rxIncomplete = false;
    ESP_LOGD(TAG, "Accumulator reset");
}

#pragma endregion

#pragma region Metadata and Playback Engine Controls

void esPod::play(bool noLoop)
{
    // Transition internal playback engine state to PLAYING
    playStatus = PB_STATE_PLAYING;

    // Trigger external playback controller callback unless internal loop update is requested
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_PLAY);
    }
    ESP_LOGI(TAG, "Engine state -> PLAY");
}

void esPod::pause(bool noLoop)
{
    // Transition internal playback engine state to PAUSED
    playStatus = PB_STATE_PAUSED;

    // Trigger external play controller (e.g., A2DP sink pause)
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_PAUSE);
    }
    ESP_LOGI(TAG, "Engine state -> PAUSE");
}

void esPod::stop(bool noLoop)
{
    // Transition internal engine state to STOPPED
    playStatus = PB_STATE_STOPPED;

    // Trigger external play controller (e.g., A2DP sink stop)
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_STOP);
    }
    ESP_LOGI(TAG, "Engine state -> STOP");
}

void esPod::updatePlayPosition(uint32_t position)
{
    // Update active play position in milliseconds
    playPosition = position;

    // Push periodic play position updates (0x04) to host if notifications are subscribed and no track change is pending
    if (playStatusNotificationState == NOTIF_ON && trackChangeAckPending == 0x00)
    {
        L0x04::_0x27_PlayStatusNotification(this, 0x04, playPosition);
    }
}

void esPod::updateAlbumName(const char *incAlbumName)
{
    if (incAlbumName)
    {
        if (trackChangeAckPending > 0x00)
        {
            if (!_albumNameUpdated)
            {
                strncpy(albumName, incAlbumName, sizeof(albumName) - 1);
                albumName[sizeof(albumName) - 1] = '\0';
                _albumNameUpdated = true;
                ESP_LOGI(TAG, "Album updated to: %s", albumName);
            }
            else
            {
                ESP_LOGD(TAG, "Album already updated to: %s", albumName);
            }
        }
        else
        {
            if (strcmp(incAlbumName, albumName) != 0)
            {
                strncpy(prevAlbumName, albumName, sizeof(prevAlbumName) - 1);
                prevAlbumName[sizeof(prevAlbumName) - 1] = '\0';
                strncpy(albumName, incAlbumName, sizeof(albumName) - 1);
                albumName[sizeof(albumName) - 1] = '\0';
                _albumNameUpdated = true;
                ESP_LOGI(TAG, "Album updated to: %s", albumName);
            }
            else
            {
                ESP_LOGD(TAG, "Album already updated to: %s", albumName);
            }
        }
        _checkAllMetaUpdated();
    }
}

void esPod::updateArtistName(const char *incArtistName)
{
    if (incArtistName)
    {
        if (trackChangeAckPending > 0x00)
        {
            if (!_artistNameUpdated)
            {
                strncpy(artistName, incArtistName, sizeof(artistName) - 1);
                artistName[sizeof(artistName) - 1] = '\0';
                _artistNameUpdated = true;
                ESP_LOGI(TAG, "Artist updated to: %s", artistName);
            }
            else
            {
                ESP_LOGD(TAG, "Artist already updated to: %s", artistName);
            }
        }
        else
        {
            if (strcmp(incArtistName, artistName) != 0)
            {
                strncpy(prevArtistName, artistName, sizeof(prevArtistName) - 1);
                prevArtistName[sizeof(prevArtistName) - 1] = '\0';
                strncpy(artistName, incArtistName, sizeof(artistName) - 1);
                artistName[sizeof(artistName) - 1] = '\0';
                _artistNameUpdated = true;
                ESP_LOGI(TAG, "Artist updated to: %s", artistName);
            }
            else
            {
                ESP_LOGD(TAG, "Artist already updated to: %s", artistName);
            }
        }
        _checkAllMetaUpdated();
    }
}

void esPod::updateTrackTitle(const char *incTrackTitle)
{
    if (incTrackTitle)
    {
        if (trackChangeAckPending > 0x00)
        {
            if (!_trackTitleUpdated)
            {
                strncpy(trackTitle, incTrackTitle, sizeof(trackTitle) - 1);
                trackTitle[sizeof(trackTitle) - 1] = '\0';
                _trackTitleUpdated = true;
                ESP_LOGI(TAG, "Title updated to: %s", trackTitle);
            }
            else
            {
                ESP_LOGD(TAG, "Title already updated to: %s", trackTitle);
            }
        }
        else
        {
            if (strcmp(incTrackTitle, prevTrackTitle) == 0)
            {
                // Spontaneous PREVIOUS track action detected from phone
                trackListPosition = (trackListPosition + TOTAL_NUM_TRACKS - 1) % TOTAL_NUM_TRACKS;
                uint32_t tempIndex = currentTrackIndex;
                currentTrackIndex = prevTrackIndex;
                prevTrackIndex = tempIndex;
                trackList[trackListPosition] = currentTrackIndex;

                strncpy(prevTrackTitle, trackTitle, sizeof(prevTrackTitle) - 1);
                prevTrackTitle[sizeof(prevTrackTitle) - 1] = '\0';
                strncpy(trackTitle, incTrackTitle, sizeof(trackTitle) - 1);
                trackTitle[sizeof(trackTitle) - 1] = '\0';
                _trackTitleUpdated = true;
                ESP_LOGI(TAG, "Title updated (PREV detected) to: %s (trackIndex: %lu, prevIndex: %lu)",
                         trackTitle, (unsigned long)currentTrackIndex, (unsigned long)prevTrackIndex);
            }
            else if (strcmp(incTrackTitle, trackTitle) != 0)
            {
                // Spontaneous NEXT / new track action detected from phone
                trackListPosition = (trackListPosition + 1) % TOTAL_NUM_TRACKS;
                prevTrackIndex = currentTrackIndex;
                currentTrackIndex = (currentTrackIndex + 1) % TOTAL_NUM_TRACKS;
                trackList[trackListPosition] = currentTrackIndex;

                strncpy(prevTrackTitle, trackTitle, sizeof(prevTrackTitle) - 1);
                prevTrackTitle[sizeof(prevTrackTitle) - 1] = '\0';
                strncpy(trackTitle, incTrackTitle, sizeof(trackTitle) - 1);
                trackTitle[sizeof(trackTitle) - 1] = '\0';
                _trackTitleUpdated = true;
                ESP_LOGI(TAG, "Title updated (NEXT detected) to: %s (trackIndex: %lu, prevIndex: %lu)",
                         trackTitle, (unsigned long)currentTrackIndex, (unsigned long)prevTrackIndex);
            }
            else
            {
                ESP_LOGD(TAG, "Title already updated to: %s", trackTitle);
            }
        }
        _checkAllMetaUpdated();
    }
}

void esPod::updateTrackDuration(uint32_t incTrackDuration)
{
    if (trackChangeAckPending > 0x00)
    {
        if (!_trackDurationUpdated)
        {
            trackDuration = incTrackDuration;
            _trackDurationUpdated = true;
            ESP_LOGI(TAG, "Track duration updated to: %lu ms", (unsigned long)trackDuration);
        }
        else
        {
            ESP_LOGD(TAG, "Track duration already updated to: %lu ms", (unsigned long)trackDuration);
        }
    }
    else
    {
        if (trackDuration != incTrackDuration)
        {
            prevTrackDuration = trackDuration;
            trackDuration = incTrackDuration;
            _trackDurationUpdated = true;
            ESP_LOGI(TAG, "Track duration updated to: %lu ms", (unsigned long)trackDuration);
        }
        else
        {
            ESP_LOGD(TAG, "Track duration already updated to: %lu ms", (unsigned long)trackDuration);
        }
    }
    _checkAllMetaUpdated();
}

void esPod::_checkAllMetaUpdated()
{
    // Aggregate metadata gatekeeper: wait until ALL 4 attributes have arrived
    if (_albumNameUpdated && _artistNameUpdated && _trackTitleUpdated && _trackDurationUpdated)
    {
        // If a track change command was waiting on metadata update, release the pending iPod ACK
        if (trackChangeAckPending > 0x00)
        {
            if (trackChangeAckPending == 0x11)
            {
                L0x03::_0x00_iPodAck(this, iPodAck_OK, trackChangeAckPending);
            }
            else
            {
                L0x04::_0x01_iPodAck(this, iPodAck_OK, trackChangeAckPending);
            }
            trackChangeAckPending = 0x00;
        }

        _albumNameUpdated = false;
        _artistNameUpdated = false;
        _trackTitleUpdated = false;
        _trackDurationUpdated = false;

        // Notify connected accessory host over Extended Interface Lingo (0x04) of the track change
        if (playStatusNotificationState == NOTIF_ON)
        {
            ESP_LOGI(TAG, "Notifying car of track change (index: %lu)", (unsigned long)currentTrackIndex);
            L0x04::_0x27_PlayStatusNotification(this, 0x01, currentTrackIndex);
        }
    }
}

#pragma endregion

#pragma region FreeRTOS Stack Initialization & Tasks

esp_err_t esPod::_initFreeRTOSStack()
{
    _cmdRingBuffer = xRingbufferCreate(CMD_RING_BUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    _txFreeBufferQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(uint8_t *));
    _txQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(aapCommand));
    _timerQueue = xQueueCreate(TIMER_QUEUE_SIZE, sizeof(TimerCallbackMessage));

    if (!_cmdRingBuffer || !_txFreeBufferQueue || !_txQueue || !_timerQueue)
    {
        ESP_LOGE(TAG, "Failed to create FreeRTOS queues/buffers");
        return ESP_FAIL;
    }

    for (int i = 0; i < TX_QUEUE_SIZE; i++)
    {
        uint8_t *bufPtr = _txBufferPool[i];
        xQueueSend(_txFreeBufferQueue, &bufPtr, 0);
    }

    _pendingTimer_0x00 = xTimerCreate("pTimer0x00", pdMS_TO_TICKS(100), pdFALSE, (void *)this, _pendingTimerCallback_0x00);
    _pendingTimer_0x03 = xTimerCreate("pTimer0x03", pdMS_TO_TICKS(100), pdFALSE, (void *)this, _pendingTimerCallback_0x03);
    _pendingTimer_0x04 = xTimerCreate("pTimer0x04", pdMS_TO_TICKS(100), pdFALSE, (void *)this, _pendingTimerCallback_0x04);

    xTaskCreate(_rxTask, "_rxTask", RX_TASK_STACK_SIZE, this, RX_TASK_PRIORITY, &_rxTaskHandle);
    xTaskCreate(_processTask, "_processTask", PROCESS_TASK_STACK_SIZE, this, PROCESS_TASK_PRIORITY, &_processTaskHandle);
    xTaskCreate(_txTask, "_txTask", TX_TASK_STACK_SIZE, this, TX_TASK_PRIORITY, &_txTaskHandle);
    xTaskCreate(_timerTask, "_timerTask", TIMER_TASK_STACK_SIZE, this, TIMER_TASK_PRIORITY, &_timerTaskHandle);

    return ESP_OK;
}

void esPod::_rxTask(void *pvParameters)
{
    esPod *esp = (esPod *)pvParameters;
    uint8_t rxBuf[MAX_PACKET_SIZE];
    bool serialTimedOut = false;

    while (1)
    {
        // Dynamic wait time based on packet ingestion state:
        // - If an incomplete frame is buffered in accumulator: wait up to INTERBYTE_TIMEOUT (500 ms)
        // - Else if serial idle timeout already triggered and debounced: wait indefinitely (portMAX_DELAY)
        // - Else: wait up to SERIAL_TIMEOUT (8000 ms)
        TickType_t waitTime = esp->_rxIncomplete
            ? pdMS_TO_TICKS(INTERBYTE_TIMEOUT)
            : (serialTimedOut ? portMAX_DELAY : pdMS_TO_TICKS(SERIAL_TIMEOUT));

        // Mode 1: External Direct-Memory Transport (e.g. TinyUSB Bulk OUT via attachRxHandler)
        if (esp->_rawRxHandler != nullptr)
        {
            if (ulTaskNotifyTake(pdTRUE, waitTime) != 0)
            {
                serialTimedOut = false;
                while (1)
                {
                    uint32_t rxBytes = esp->_rawRxHandler(rxBuf, sizeof(rxBuf));
                    if (rxBytes == 0) break;
                    esp->processRawBuffer(rxBuf, rxBytes);
                }
            }
            else
            {
                // Timeout fired on lack of notification
                if (esp->_rxIncomplete)
                {
                    ESP_LOGW(TAG, "Accumulator incomplete packet timeout (%u ms), discarding partial frame", (unsigned int)INTERBYTE_TIMEOUT);
                    esp->resetAccumulator();
                }
                else if (!serialTimedOut)
                {
                    ESP_LOGW(TAG, "Serial idle timeout (%u ms), resetting esPod state machine", (unsigned int)SERIAL_TIMEOUT);
                    esp->resetState();
                    serialTimedOut = true;
                }
            }
        }
        // Mode 2: Hardware UART mode (when physical UART pins are assigned)
        else if (esp->_rxPin >= 0 && esp->_txPin >= 0 && uart_is_driver_installed(esp->_uartPort))
        {
            int rxLen = uart_read_bytes(esp->_uartPort, rxBuf, sizeof(rxBuf), waitTime);
            if (rxLen > 0)
            {
                serialTimedOut = false;
                esp->processRawBuffer(rxBuf, (size_t)rxLen);
            }
            else
            {
                if (esp->_rxIncomplete)
                {
                    ESP_LOGW(TAG, "Packet incomplete, discarding");
                    esp->resetAccumulator();
                }
                else if (!serialTimedOut)
                {
                    ESP_LOGW(TAG, "No activity in %lu ms, resetting RX state", (unsigned long)SERIAL_TIMEOUT);
                    esp->resetState();
                    serialTimedOut = true;
                }
            }
        }
        else
        {
            // Yield CPU if no transport is configured
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void esPod::_processTask(void *pvParameters)
{
    esPod *esp = (esPod *)pvParameters;
    size_t itemSize = 0;

    while (1)
    {
        // Block indefinitely until raw iAP frame data arrives in command ringbuffer
        uint8_t *item = (uint8_t *)xRingbufferReceive(esp->_cmdRingBuffer, &itemSize, portMAX_DELAY);
        if (item != NULL && itemSize > 0)
        {
            // Only parse frame and dispatch to Lingo handlers if esPod is enabled
            if (!esp->disabled)
            {
                esp->_processPacket(item, itemSize);
            }
            else
            {
                ESP_LOGD(TAG, "_processTask: ignoring %u bytes because esPod is disabled", (unsigned int)itemSize);
            }

            // Return item back to ringbuffer storage pool
            vRingbufferReturnItem(esp->_cmdRingBuffer, (void *)item);
        }
    }
}

void esPod::_txTask(void *pvParameters)
{
    esPod *esp = (esPod *)pvParameters;
    aapCommand txCmd;

    while (1)
    {
        // Block waiting for outbound iAP response packets queued by Lingo handlers
        if (xQueueReceive(esp->_txQueue, &txCmd, portMAX_DELAY) == pdTRUE)
        {
            if (txCmd.payload != nullptr && txCmd.length > 0)
            {
                // Only transmit outbound frame if esPod is active and enabled
                if (!esp->disabled)
                {
                    // Route outbound frame: priority to attached custom transport callback (e.g. TinyUSB PL2303)
                    if (esp->_rawTxHandler != nullptr)
                    {
                        esp->_rawTxHandler(txCmd.payload, txCmd.length);
                    }
                    else if (esp->_rxPin >= 0 && esp->_txPin >= 0 && uart_is_driver_installed(esp->_uartPort))
                    {
                        // Fallback to physical hardware UART pins if assigned
                        uart_write_bytes(esp->_uartPort, (const char *)txCmd.payload, txCmd.length);
                    }
                }
                else
                {
                    ESP_LOGD(TAG, "_txTask: dropping outbound frame (%u bytes) because esPod is disabled", (unsigned int)txCmd.length);
                }

                // Recast and return payload buffer pointer to static free buffer pool
                uint8_t *bufPtr = txCmd.payload;
                xQueueSend(esp->_txFreeBufferQueue, &bufPtr, 0);
            }
        }
    }
}

void esPod::_timerTask(void *pvParameters)
{
    esPod *esp = (esPod *)pvParameters;
    TimerCallbackMessage msg;

    while (1)
    {
        // Block waiting for software timer callback messages (delayed ACKs)
        if (xQueueReceive(esp->_timerQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            // Only dispatch delayed iPod ACK responses if esPod is enabled
            if (!esp->disabled)
            {
                if (msg.cmdID == esp->trackChangeAckPending)
                {
                    esp->trackChangeAckPending = 0x00;
                    esp->_albumNameUpdated = false;
                    esp->_artistNameUpdated = false;
                    esp->_trackTitleUpdated = false;
                    esp->_trackDurationUpdated = false;
                }

                switch (msg.targetLingo)
                {
                case 0x00:
                    L0x00::_0x02_iPodAck(esp, iPodAck_OK, msg.cmdID);
                    break;
                case 0x03:
                    L0x03::_0x00_iPodAck(esp, iPodAck_OK, msg.cmdID);
                    break;
                case 0x04:
                    L0x04::_0x01_iPodAck(esp, iPodAck_OK, msg.cmdID);
                    break;
                }
                ESP_LOGI(TAG, "Timer expired for Lingo 0x%02X cmd 0x%02X -> Sent iPodAck_OK", msg.targetLingo, msg.cmdID);
            }
        }
    }
}

#pragma endregion

#pragma region Timer Callbacks & Packet Helpers

void esPod::_pendingTimerCallback_0x00(TimerHandle_t xTimer)
{
    esPod *esp = (esPod *)pvTimerGetTimerID(xTimer);
    TimerCallbackMessage msg = {esp->_pendingCmdId_0x00, 0x00};
    xQueueSend(esp->_timerQueue, &msg, 0);
}

void esPod::_pendingTimerCallback_0x03(TimerHandle_t xTimer)
{
    esPod *esp = (esPod *)pvTimerGetTimerID(xTimer);
    TimerCallbackMessage msg = {esp->_pendingCmdId_0x03, 0x03};
    xQueueSend(esp->_timerQueue, &msg, 0);
}

void esPod::_pendingTimerCallback_0x04(TimerHandle_t xTimer)
{
    esPod *esp = (esPod *)pvTimerGetTimerID(xTimer);
    TimerCallbackMessage msg = {esp->_pendingCmdId_0x04, 0x04};
    xQueueSend(esp->_timerQueue, &msg, 0);
}

uint8_t esPod::_checksum(const uint8_t *byteArray, uint32_t len)
{
    uint32_t tempChecksum = len;
    for (uint32_t i = 0; i < len; i++)
    {
        tempChecksum += byteArray[i];
    }
    tempChecksum = 0x100 - (tempChecksum & 0xFF);
    return (uint8_t)tempChecksum;
}

void esPod::_sendPacket(const uint8_t *byteArray, uint32_t len)
{
    _queuePacket(byteArray, len);
}

void esPod::_queuePacket(const uint8_t *byteArray, uint32_t len)
{
    uint8_t *bufPtr = nullptr;
    if (xQueueReceive(_txFreeBufferQueue, &bufPtr, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        bufPtr[0] = 0xFF;
        bufPtr[1] = 0x55;
        bufPtr[2] = (uint8_t)len;
        memcpy(&bufPtr[3], byteArray, len);
        bufPtr[3 + len] = _checksum(byteArray, len);

        aapCommand cmd = {bufPtr, 3 + len + 1};
        ESP_LOGI(TAG, "TX Packet queued: %u bytes (Lingo 0x%02X, payload %lu bytes)", (unsigned int)(3 + len + 1), byteArray[0], (unsigned long)len);
        xQueueSend(_txQueue, &cmd, 0);
    }
}

void esPod::_queuePacketToFront(const uint8_t *byteArray, uint32_t len)
{
    uint8_t *bufPtr = nullptr;
    if (xQueueReceive(_txFreeBufferQueue, &bufPtr, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        bufPtr[0] = 0xFF;
        bufPtr[1] = 0x55;
        bufPtr[2] = (uint8_t)len;
        memcpy(&bufPtr[3], byteArray, len);
        bufPtr[3 + len] = _checksum(byteArray, len);

        aapCommand cmd = {bufPtr, 3 + len + 1};
        ESP_LOGI(TAG, "TX Packet queued (front): %u bytes (Lingo 0x%02X, payload %lu bytes)", (unsigned int)(3 + len + 1), byteArray[0], (unsigned long)len);
        xQueueSendToFront(_txQueue, &cmd, 0);
    }
}

void esPod::_processPacket(const uint8_t *byteArray, size_t len)
{
    // Validate minimal iAP frame length (header + len + lingo + checksum = 5 bytes min) and preamble 0xFF 0x55
    if (len < 5 || byteArray[0] != 0xFF || byteArray[1] != 0x55)
        return;

    uint8_t payloadLen = byteArray[2];
    const uint8_t *lingoPtr = &byteArray[3];

    // Compute frame checksum starting at Lingo ID
    uint8_t calcSum = _checksum(lingoPtr, payloadLen);
    uint8_t rxSum = byteArray[3 + payloadLen];

    // Verify checksum matches expected frame trailer
    if (calcSum != rxSum)
    {
        ESP_LOGE(TAG, "Checksum error: calc 0x%02x vs rx 0x%02x", calcSum, rxSum);
        return;
    }

    uint8_t lingoID = lingoPtr[0];
    const uint8_t *cmdData = &lingoPtr[1];
    uint32_t cmdLen = payloadLen - 1;

    ESP_LOGI(TAG, "RX iAP Frame: Lingo 0x%02X, CMD 0x%02X (payload %lu bytes)", lingoID, cmdData[0], (unsigned long)cmdLen);

    // Route command payload to target Lingo state machine handler
    switch (lingoID)
    {
    case 0x00:
        L0x00::processLingo(this, cmdData, cmdLen);
        break;
    case 0x03:
        L0x03::processLingo(this, cmdData, cmdLen);
        break;
    case 0x04:
        L0x04::processLingo(this, cmdData, cmdLen);
        break;
    default:
        ESP_LOGW(TAG, "Unsupported Lingo ID: 0x%02x", lingoID);
        break;
    }
}

#pragma endregion
