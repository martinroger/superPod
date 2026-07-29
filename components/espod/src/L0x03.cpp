/**
 * @file L0x03.cpp
 * @brief Simple Remote Lingo 0x03 Command Implementation.
 */

#include "L0x03.h"
#include "esPod.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "L0x03";

static inline uint32_t get_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/// @brief Parses and dispatches incoming Lingo 0x03 commands
void L0x03::processLingo(esPod *esp, const uint8_t *byteArray, uint32_t len)
{
    uint8_t cmdID = byteArray[0];
    uint32_t currentEQProfileIndex, tempTrackIndex;

    switch (cmdID)
    {
    case L0x03_GetCurrentEQProfileIndex:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetCurrentEQProfileIndex", cmdID);
        L0x03::_0x02_RetCurrentEQProfileIndex(esp);
    }
    break;

    case L0x03_SetCurrentEQProfileIndex:
    {
        currentEQProfileIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[1]));
        ESP_LOGI(TAG, "CMD: 0x%02x SetCurrentEQProfileIndex 0x%02lx", cmdID, currentEQProfileIndex);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetNumEQProfiles:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetNumEQProfiles", cmdID);
        L0x03::_0x05_RetNumEQProfiles(esp);
    }
    break;

    case L0x03_GetIndexedEQProfileName:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetIndexedEQProfileName", cmdID);
        L0x03::_0x07_RetIndexedEQProfileName(esp);
    }
    break;

    case L0x03_SetRemoteEventNotification:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x SetRemoteEventNotification", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
        L0x03::_0x09_RemoteEventNotification(esp);
    }
    break;

    case L0x03_GetRemoteEventStatus:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetRemoteEventStatus", cmdID);
        L0x03::_0x0B_RetRemoteEventStatus(esp, 0);
    }
    break;

    case L0x03_GetiPodStateInfo:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetiPodStateInfo", cmdID);
        L0x03::_0x0D_RetiPodStateInfo(esp);
    }
    break;

    case L0x03_SetiPodStateInfo:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x SetiPodStateInfo", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetPlayStatus:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetPlayStatus", cmdID);
        L0x03::_0x10_RetPlayStatus(esp, esp->playStatus, esp->currentTrackIndex, esp->trackDuration, esp->playPosition);
    }
    break;

    case L0x03_SetCurrentPlayingTrack:
    {
        tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[1]));
        ESP_LOGI(TAG, "CMD: 0x%02x SetCurrentPlayingTrack index %lu", cmdID, tempTrackIndex);
        if (esp->playStatus != PB_STATE_PLAYING)
        {
            esp->play();
        }
        if (tempTrackIndex == esp->trackList[(esp->trackListPosition + TOTAL_NUM_TRACKS - 1) % TOTAL_NUM_TRACKS])
        {
            esp->prevTrackIndex = esp->currentTrackIndex;
            strcpy(esp->prevAlbumName, esp->albumName);
            strcpy(esp->prevArtistName, esp->artistName);
            strcpy(esp->prevTrackTitle, esp->trackTitle);
            esp->prevTrackDuration = esp->trackDuration;

            esp->trackListPosition = (esp->trackListPosition + TOTAL_NUM_TRACKS - 1) % TOTAL_NUM_TRACKS;
            esp->currentTrackIndex = tempTrackIndex;

            esp->trackChangeAckPending = cmdID;
            esp->trackChangeTimestamp = get_millis();
            L0x03::_0x00_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

            if (esp->_playStatusHandler)
                esp->_playStatusHandler(PB_CMD_PREVIOUS_TRACK);
        }
        else if (tempTrackIndex == esp->currentTrackIndex)
        {
            ESP_LOGD(TAG, "Selected same track as current: %lu", tempTrackIndex);
            L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);

            if (esp->_playStatusHandler)
                esp->_playStatusHandler(PB_CMD_PREV);
        }
        else
        {
            esp->prevTrackIndex = esp->currentTrackIndex;
            strcpy(esp->prevAlbumName, esp->albumName);
            strcpy(esp->prevArtistName, esp->artistName);
            strcpy(esp->prevTrackTitle, esp->trackTitle);
            esp->prevTrackDuration = esp->trackDuration;

            esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
            esp->trackList[esp->trackListPosition] = tempTrackIndex;
            esp->currentTrackIndex = tempTrackIndex;

            esp->trackChangeAckPending = cmdID;
            esp->trackChangeTimestamp = get_millis();
            L0x03::_0x00_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

            if (esp->_playStatusHandler)
                esp->_playStatusHandler(PB_CMD_NEXT_TRACK);
        }
    }
    break;

    case L0x03_GetIndexedPlayingTrackInfo:
    {
        tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
        switch (byteArray[1])
        {
        case 0x00:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Duration", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, (uint32_t)esp->prevTrackDuration);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, (uint32_t)esp->trackDuration);
            }
            break;

        case 0x02:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Artist", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevArtistName);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->artistName);
            }
            break;

        case 0x03:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Album", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevAlbumName);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->albumName);
            }
            break;

        case 0x04:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Genre", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->trackGenre);
            break;

        case 0x05:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Title", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->prevTrackTitle);
            }
            else
            {
                L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->trackTitle);
            }
            break;

        case 0x06:
            ESP_LOGI(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Composer", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x13_RetIndexedPlayingTrackInfo(esp, byteArray[1], esp->composer);
            break;

        default:
            ESP_LOGW(TAG, "CMD 0x%02x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Type not recognised!", cmdID, byteArray[1], tempTrackIndex, esp->prevTrackIndex);
            L0x03::_0x00_iPodAck(esp, iPodAck_BadParam, cmdID);
            break;
        }
    }
    break;

    case L0x03_GetNumPlayingTracks:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetNumPlayingTracks", cmdID);
        L0x03::_0x15_RetNumPlayingTracks(esp, TOTAL_NUM_TRACKS);
    }
    break;

    case L0x03_GetArtworkFormats:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetArtworkFormats", cmdID);
        L0x03::_0x17_RetArtworkFormats(esp);
    }
    break;

    case L0x03_GetTrackArtworkData:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetTrackArtworkData", cmdID);
        L0x03::_0x19_RetTrackArtworkData(esp);
    }
    break;

    case L0x03_GetPowerBatteryState:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetPowerBatteryState", cmdID);
        L0x03::_0x1B_RetPowerBatteryState(esp, 0x05);
    }
    break;

    case L0x03_GetSoundCheckState:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetSoundCheckState", cmdID);
        L0x03::_0x1D_RetSoundCheckState(esp, 0x00);
    }
    break;

    case L0x03_SetSoundCheckState:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x SetSoundCheckState", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x03_GetTrackArtworkTimes:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetTrackArtworkTimes", cmdID);
        L0x03::_0x20_RetTrackArtworkTimes(esp);
    }
    break;

    case L0x03_CreateGeniusPlaylist:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x CreateGeniusPlaylist", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_SelNotGenius, cmdID);
    }
    break;

    case L0x03_IsGeniusAvailableForTrack:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x IsGeniusAvailableForTrack", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_SelNotGenius, cmdID);
    }
    break;

    default:
    {
        ESP_LOGW(TAG, "CMD 0x%02x not recognized.", cmdID);
        L0x03::_0x00_iPodAck(esp, iPodAck_CmdFailed, cmdID);
    }
    break;
    }
}

void L0x03::_0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%02x", ackCode, cmdID);
    const uint8_t txPacket[] = {0x03, 0x00, (uint8_t)ackCode, cmdID};
    if (cmdID == esp->_pendingCmdId_0x03)
    {
        stopTimer(esp->_pendingTimer_0x03);
        esp->_pendingCmdId_0x03 = 0x00;
        esp->_queuePacketToFront(txPacket, sizeof(txPacket));
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x00_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID, uint32_t numField)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%02x Numfield: %lu", ackCode, cmdID, numField);
    uint8_t txPacket[8] = {0x03, 0x00, (uint8_t)ackCode, cmdID};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, sizeof(txPacket));
    esp->_pendingCmdId_0x03 = cmdID;
    startTimer(esp->_pendingTimer_0x03, numField);
}

void L0x03::_0x02_RetCurrentEQProfileIndex(esPod *esp)
{
    ESP_LOGI(TAG, "Return EQ Profile Index");
    const uint8_t txPacket[] = {0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x05_RetNumEQProfiles(esPod *esp)
{
    ESP_LOGI(TAG, "Return Num Profiles: 1");
    uint8_t txPacket[] = {0x03, 0x05, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[2]) = swap_endian<uint32_t>(1);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x07_RetIndexedEQProfileName(esPod *esp)
{
    const char *EQProfileName = "Base EQ";
    ESP_LOGI(TAG, "Return EQ name: %s", EQProfileName);
    uint8_t txPacket[255] = {0x03, 0x07};
    strcpy((char *)&txPacket[2], EQProfileName);
    esp->_queuePacket(txPacket, 3 + strlen(EQProfileName));
}

void L0x03::_0x09_RemoteEventNotification(esPod *esp)
{
    ESP_LOGW(TAG, "RemoteEventNotification not implemented");
}

void L0x03::_0x0B_RetRemoteEventStatus(esPod *esp, uint32_t remEventStatus)
{
    ESP_LOGW(TAG, "RetRemoteEventStatus not implemented");
}

void L0x03::_0x0D_RetiPodStateInfo(esPod *esp)
{
    ESP_LOGW(TAG, "RetiPodStateInfo not implemented");
}

void L0x03::_0x10_RetPlayStatus(esPod *esp, uint8_t playState, uint32_t trackIndex, uint32_t trackTotMs, uint32_t trackPosMs)
{
    ESP_LOGI(TAG, "Play status 0x%02x of index %lu at pos %lu / %lu ms", playState, trackIndex, trackPosMs, trackTotMs);
    uint8_t txPacket[] = {
        0x03, 0x10, playState,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(trackTotMs);
    *((uint32_t *)&txPacket[11]) = swap_endian<uint32_t>(trackPosMs);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x13_RetIndexedPlayingTrackInfo(esPod *esp, uint8_t trackInfoType, char *trackInfoChars)
{
    ESP_LOGI(TAG, "Req'd track info type: 0x%02x", trackInfoType);
    uint8_t txPacket[255] = {0x03, 0x13, trackInfoType};
    strcpy((char *)&txPacket[3], trackInfoChars);
    esp->_queuePacket(txPacket, 3 + strlen(trackInfoChars) + 1);
}

void L0x03::_0x13_RetIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms)
{
    ESP_LOGI(TAG, "Track duration: %lu", trackDuration_ms);
    uint8_t txPacket[13] = {
        0x03, 0x13, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00};
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(trackDuration_ms);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x15_RetNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks)
{
    ESP_LOGI(TAG, "Playing tracks: %lu", numPlayingTracks);
    uint8_t txPacket[] = {0x03, 0x15, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[2]) = swap_endian<uint32_t>(numPlayingTracks);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x17_RetArtworkFormats(esPod *esp)
{
    ESP_LOGW(TAG, "RetArtworkFormats not implemented");
}

void L0x03::_0x19_RetTrackArtworkData(esPod *esp)
{
    ESP_LOGW(TAG, "RetTrackArtworkData not implemented");
}

void L0x03::_0x1B_RetPowerBatteryState(esPod *esp, uint8_t powerBatteryState)
{
    ESP_LOGI(TAG, "RetPowerBatteryState 0x%02x", powerBatteryState);
    uint8_t batteryLevel = 0x00;
    switch (powerBatteryState)
    {
    case 0x00:
        batteryLevel = (uint8_t)((29.0 / 100.0) * 255.0);
        break;
    case 0x01:
        batteryLevel = 255 / 2;
        break;
    default:
        batteryLevel = 0xFF;
        break;
    }

    uint8_t txPacket[] = {0x03, 0x1B, powerBatteryState, batteryLevel};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x1D_RetSoundCheckState(esPod *esp, uint8_t soundCheckState)
{
    ESP_LOGI(TAG, "RetSoundCheckState 0x%02x", soundCheckState);
    uint8_t txPacket[] = {0x03, 0x1D, soundCheckState};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x03::_0x20_RetTrackArtworkTimes(esPod *esp)
{
    ESP_LOGW(TAG, "RetTrackArtworkTimes not implemented");
}
