/**
 * @file esPod_utils.h
 * @brief Enums, data structures, endianness helpers, and FreeRTOS timer utility functions for esPod.
 */

#pragma once

#include <cstdint>
#include <climits>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esPod_conf.h"

#ifdef ARDUINO
typedef uint8_t byte;
#else
typedef uint8_t byte;
#endif

#pragma region ENUMS

/**
 * @brief iPod Acknowledgement Response Codes (Apple Accessory Protocol)
 */
enum IPOD_ACK_CODE : uint8_t
{
    iPodAck_OK = 0x00,
    iPodAck_UnknownDBCat = 0x01,
    iPodAck_CmdFailed = 0x02,
    iPodAck_OutOfResources = 0x03,
    iPodAck_BadParam = 0x04,
    iPodAck_UnknownID = 0x05,
    iPodAck_CmdPending = 0x06,
    iPodAck_NotAuthenticated = 0x07,
    iPodAck_BadAuthVersion = 0x08,
    iPodAck_AccPowerModeReqFailed = 0x09,
    iPodAck_CertificateInvalid = 0x0A,
    iPodAck_CertPermissionsInvalid = 0x0B,
    iPodAck_FileInUse = 0x0C,
    iPodAck_FileHndlInvalid = 0x0D,
    iPodAck_DirNotEmpty = 0x0E,
    iPodAck_TimedOut = 0x0F,
    iPodAck_CmdUnavail = 0x10,
    iPodAck_DetectFloat_BadResistor = 0x11,
    iPodAck_SelNotGenius = 0x12,
    iPodAck_MultiDataSection_OK = 0x13,
    iPodAck_LingoBusy = 0x14,
    iPodAck_MaxConnections = 0x15,
    iPodAck_HIDAlreadyInUse = 0x16,
    iPodAck_DroppedData = 0x17,
    iPodAck_OutModeError = 0x18
};

/**
 * @brief Playback State Indicators
 */
enum PB_STATUS : uint8_t
{
    PB_STATE_STOPPED = 0x00,
    PB_STATE_PLAYING = 0x01,
    PB_STATE_PAUSED = 0x02,
    PB_STATE_ERROR = 0xFF
};

/**
 * @brief Playback Engine Control Commands
 */
enum PB_COMMAND : uint8_t
{
    PB_CMD_TOGGLE = 0x01,
    PB_CMD_STOP = 0x02,
    PB_CMD_NEXT_TRACK = 0x03,
    PB_CMD_PREVIOUS_TRACK = 0x04,
    PB_CMD_SEEK_FF = 0x05,
    PB_CMD_SEEK_RW = 0x06,
    PB_CMD_STOP_SEEK = 0x07,
    PB_CMD_NEXT = 0x08,
    PB_CMD_PREV = 0x09,
    PB_CMD_PLAY = 0x0A,
    PB_CMD_PAUSE = 0x0B
};

/**
 * @brief Database Category Identifiers
 */
enum DB_CATEGORY : uint8_t
{
    DB_CAT_PLAYLIST = 0x01,
    DB_CAT_ARTIST = 0x02,
    DB_CAT_ALBUM = 0x03,
    DB_CAT_GENRE = 0x04,
    DB_CAT_TRACK = 0x05,
    DB_CAT_COMPOSER = 0x06,
    DB_CAT_AUDIOBOOK = 0x07,
    DB_CAT_PODCAST = 0x08
};

/**
 * @brief Notification Engine States
 */
enum NOTIF_STATES : uint8_t
{
    NOTIF_OFF = 0x00,
    NOTIF_ON = 0x01
};

#pragma endregion

/**
 * @brief Container structure for an Apple Accessory Protocol (AAP) command packet.
 */
struct aapCommand
{
    uint8_t *payload = nullptr;
    uint32_t length = 0;
};

/**
 * @brief Container for FreeRTOS software timer ack notifications.
 */
struct TimerCallbackMessage
{
    uint8_t cmdID;       // Command ID to be acknowledged
    uint8_t targetLingo; // Target Lingo ID
};

#pragma region Local utilities

/**
 * @brief Swaps byte order of multi-byte integers between Little-Endian (ESP32) and Big-Endian (iPod iAP).
 * 
 * @tparam T Integer type to swap.
 * @param u Value in source endianness.
 * @return T Value in target endianness.
 */
template <typename T>
T swap_endian(T u)
{
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

/**
 * @brief Starts or restarts a FreeRTOS software timer with a specified interval in milliseconds.
 * 
 * @param timer TimerHandle_t of target timer.
 * @param time_ms New interval in milliseconds. Defaults to TRACK_CHANGE_TIMEOUT.
 */
inline void startTimer(TimerHandle_t timer, unsigned long time_ms = TRACK_CHANGE_TIMEOUT)
{
    if (xTimerIsTimerActive(timer) == pdTRUE)
    {
        xTimerStop(timer, 0);
    }
    xTimerChangePeriod(timer, pdMS_TO_TICKS(time_ms), 0);
    xTimerStart(timer, 0);
}

/**
 * @brief Stops an active FreeRTOS software timer safely.
 * 
 * @param timer TimerHandle_t of target timer.
 */
inline void stopTimer(TimerHandle_t timer)
{
    if (xTimerIsTimerActive(timer) == pdTRUE)
    {
        xTimerStop(timer, 0);
    }
}

#pragma endregion
