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
    currentTrackIndex = 0;
    trackListPosition = 0;
    disabled = false;
    ESP_LOGI(TAG, "State reset clean");
}

void esPod::attachPlayControlHandler(playStatusHandler_t playHandler)
{
    _playStatusHandler = playHandler;
    ESP_LOGI(TAG, "playStatusHandler attached");
}

#pragma endregion

#pragma region Direct Raw Ingestion API

/// @brief Direct In-Memory Raw iAP Packet Processing (bypasses UART in USB Single-MCU Mode)
size_t esPod::processRawBuffer(const uint8_t *data, size_t len)
{
    if (disabled || data == nullptr || len == 0 || _cmdRingBuffer == nullptr)
        return 0;

    BaseType_t ret = xRingbufferSend(_cmdRingBuffer, (void *)data, len, pdMS_TO_TICKS(10));
    if (ret != pdTRUE)
    {
        ESP_LOGW(TAG, "cmdRingBuffer full, dropping %d raw bytes", (int)len);
        return 0;
    }
    return len;
}

#pragma endregion

#pragma region Metadata and Playback Engine Controls

void esPod::play(bool noLoop)
{
    playStatus = PB_STATE_PLAYING;
    if (playStatusNotificationState == NOTIF_ON)
    {
        L0x04::_0x27_PlayStatusNotification(this, 0x01, currentTrackIndex);
    }
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_PLAY);
    }
    ESP_LOGI(TAG, "Engine state -> PLAY");
}

void esPod::pause(bool noLoop)
{
    playStatus = PB_STATE_PAUSED;
    if (playStatusNotificationState == NOTIF_ON)
    {
        L0x04::_0x27_PlayStatusNotification(this, 0x01, currentTrackIndex);
    }
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_PAUSE);
    }
    ESP_LOGI(TAG, "Engine state -> PAUSE");
}

void esPod::stop(bool noLoop)
{
    playStatus = PB_STATE_STOPPED;
    if (playStatusNotificationState == NOTIF_ON)
    {
        L0x04::_0x27_PlayStatusNotification(this, 0x00);
    }
    if (!noLoop && _playStatusHandler != nullptr)
    {
        _playStatusHandler(PB_CMD_STOP);
    }
    ESP_LOGI(TAG, "Engine state -> STOP");
}

void esPod::updatePlayPosition(uint32_t position)
{
    playPosition = position;
    if (playStatusNotificationState == NOTIF_ON)
    {
        L0x04::_0x27_PlayStatusNotification(this, 0x04, playPosition);
    }
}

void esPod::updateAlbumName(const char *incAlbumName)
{
    if (incAlbumName && strcmp(albumName, incAlbumName) != 0)
    {
        strncpy(albumName, incAlbumName, sizeof(albumName) - 1);
        _albumNameUpdated = true;
        _checkAllMetaUpdated();
    }
}

void esPod::updateArtistName(const char *incArtistName)
{
    if (incArtistName && strcmp(artistName, incArtistName) != 0)
    {
        strncpy(artistName, incArtistName, sizeof(artistName) - 1);
        _artistNameUpdated = true;
        _checkAllMetaUpdated();
    }
}

void esPod::updateTrackTitle(const char *incTrackTitle)
{
    if (incTrackTitle && strcmp(trackTitle, incTrackTitle) != 0)
    {
        strncpy(trackTitle, incTrackTitle, sizeof(trackTitle) - 1);
        _trackTitleUpdated = true;
        _checkAllMetaUpdated();
    }
}

void esPod::updateTrackDuration(uint32_t incTrackDuration)
{
    if (trackDuration != incTrackDuration)
    {
        trackDuration = incTrackDuration;
        _trackDurationUpdated = true;
        _checkAllMetaUpdated();
    }
}

void esPod::_checkAllMetaUpdated()
{
    if (_trackTitleUpdated || _artistNameUpdated || _albumNameUpdated)
    {
        _trackTitleUpdated = false;
        _artistNameUpdated = false;
        _albumNameUpdated = false;
        _trackDurationUpdated = false;

        if (trackChangeAckPending != 0x00)
        {
            if (trackChangeAckPending <= 0xFF)
            {
                L0x00::_0x02_iPodAck(this, iPodAck_OK, (uint8_t)trackChangeAckPending);
            }
            trackChangeAckPending = 0x00;
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

    _pendingTimer_0x00 = xTimerCreate("pTimer0x00", pdMS_TO_TICKS(100), pdFALSE, (void *)0, _pendingTimerCallback_0x00);
    _pendingTimer_0x03 = xTimerCreate("pTimer0x03", pdMS_TO_TICKS(100), pdFALSE, (void *)3, _pendingTimerCallback_0x03);
    _pendingTimer_0x04 = xTimerCreate("pTimer0x04", pdMS_TO_TICKS(100), pdFALSE, (void *)4, _pendingTimerCallback_0x04);

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

    while (1)
    {
        if (esp->_rxPin >= 0 && esp->_txPin >= 0 && uart_is_driver_installed(esp->_uartPort))
        {
            int rxLen = uart_read_bytes(esp->_uartPort, rxBuf, sizeof(rxBuf), pdMS_TO_TICKS(10));
            if (rxLen > 0)
            {
                esp->processRawBuffer(rxBuf, rxLen);
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void esPod::_processTask(void *pvParameters)
{
    esPod *esp = (esPod *)pvParameters;
    size_t itemSize = 0;

    while (1)
    {
        uint8_t *item = (uint8_t *)xRingbufferReceive(esp->_cmdRingBuffer, &itemSize, portMAX_DELAY);
        if (item != NULL && itemSize > 0)
        {
            esp->_processPacket(item, itemSize);
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
        if (xQueueReceive(esp->_txQueue, &txCmd, portMAX_DELAY) == pdTRUE)
        {
            if (txCmd.payload != nullptr && txCmd.length > 0)
            {
                if (esp->_rxPin >= 0 && esp->_txPin >= 0 && uart_is_driver_installed(esp->_uartPort))
                {
                    uart_write_bytes(esp->_uartPort, (const char *)txCmd.payload, txCmd.length);
                }
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
        if (xQueueReceive(esp->_timerQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
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
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        sum += byteArray[i];
    }
    return (uint8_t)(0x100 - sum);
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
        xQueueSendToFront(_txQueue, &cmd, 0);
    }
}

void esPod::_processPacket(const uint8_t *byteArray, size_t len)
{
    if (len < 5 || byteArray[0] != 0xFF || byteArray[1] != 0x55)
        return;

    uint8_t payloadLen = byteArray[2];
    const uint8_t *lingoPtr = &byteArray[3];
    uint8_t calcSum = _checksum(lingoPtr, payloadLen);
    uint8_t rxSum = byteArray[3 + payloadLen];

    if (calcSum != rxSum)
    {
        ESP_LOGE(TAG, "Checksum error: calc 0x%02x vs rx 0x%02x", calcSum, rxSum);
        return;
    }

    uint8_t lingoID = lingoPtr[0];
    const uint8_t *cmdData = &lingoPtr[1];
    uint32_t cmdLen = payloadLen - 1;

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
