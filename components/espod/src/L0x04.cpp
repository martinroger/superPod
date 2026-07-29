/**
 * @file L0x04.cpp
 * @brief Extended Interface Lingo 0x04 Command Implementation.
 */

#include "L0x04.h"
#include "esPod.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "L0x04";

static inline uint32_t get_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/// @brief Parses and dispatches incoming Lingo 0x04 commands
void L0x04::processLingo(esPod *esp, const uint8_t *byteArray, uint32_t len)
{
    uint8_t cmdID = byteArray[1];
    uint8_t category;
    uint32_t startIndex, counts, tempTrackIndex;
    char noCat[25] = "--";

    if (!esp->extendedInterfaceModeActive)
    {
        ESP_LOGW(TAG, "CMD 0x%04x not executed : Not in extendedInterfaceMode!", cmdID);
        L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
    }
    else
    {
        switch (cmdID)
        {
        case L0x04_GetIndexedPlayingTrackInfo:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[3]));
            switch (byteArray[2])
            {
            case 0x00:
                ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Duration", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                if (tempTrackIndex == esp->prevTrackIndex)
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, (uint32_t)esp->prevTrackDuration);
                }
                else
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, (uint32_t)esp->trackDuration);
                }
                break;
            case 0x02:
                ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Release date", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], (uint16_t)2001);
                break;
            case 0x01:
                ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Title", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                if (tempTrackIndex == esp->prevTrackIndex)
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->prevTrackTitle);
                }
                else
                {
                    L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->trackTitle);
                }
                break;
            case 0x05:
                ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Genre", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->trackGenre);
                break;
            case 0x06:
                ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Composer", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esp, byteArray[2], esp->composer);
                break;
            default:
                ESP_LOGW(TAG, "CMD 0x%04x GetIndexedPlayingTrackInfo 0x%02x for index %lu (previous %lu) : Type not recognised!", cmdID, byteArray[2], tempTrackIndex, esp->prevTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
                break;
            }
        }
        break;

        case L0x04_RequestProtocolVersion:
        {
            ESP_LOGI(TAG, "CMD 0x%04x RequestProtocolVersion", cmdID);
            L0x04::_0x13_ReturnProtocolVersion(esp);
        }
        break;

        case L0x04_ResetDBSelection:
        {
            ESP_LOGI(TAG, "CMD 0x%04x ResetDBSelection", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_SelectDBRecord:
        {
            ESP_LOGI(TAG, "CMD 0x%04x SelectDBRecord", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetNumberCategorizedDBRecords:
        {
            category = byteArray[2];
            ESP_LOGI(TAG, "CMD 0x%04x GetNumberCategorizedDBRecords category: 0x%02x", cmdID, category);
            if (category == DB_CAT_TRACK)
            {
                L0x04::_0x19_ReturnNumberCategorizedDBRecords(esp, esp->totalNumberTracks);
            }
            else
            {
                L0x04::_0x19_ReturnNumberCategorizedDBRecords(esp, 1);
            }
        }
        break;

        case L0x04_RetrieveCategorizedDatabaseRecords:
        {
            category = byteArray[2];
            startIndex = swap_endian<uint32_t>(*(uint32_t *)&byteArray[3]);
            counts = swap_endian<uint32_t>(*(uint32_t *)&byteArray[7]);

            ESP_LOGI(TAG, "CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x from %lu for %lu counts", cmdID, category, startIndex, counts);
            switch (category)
            {
            case DB_CAT_PLAYLIST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->playList);
                }
                break;
            case DB_CAT_ARTIST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->artistName);
                }
                break;
            case DB_CAT_ALBUM:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->albumName);
                }
                break;
            case DB_CAT_GENRE:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->trackGenre);
                }
                break;
            case DB_CAT_TRACK:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->trackTitle);
                }
                break;
            case DB_CAT_COMPOSER:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, esp->composer);
                }
                break;
            case DB_CAT_AUDIOBOOK:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, noCat);
                }
                break;
            case DB_CAT_PODCAST:
                for (uint32_t i = startIndex; i < startIndex + counts; i++)
                {
                    L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esp, i, noCat);
                }
                break;
            default:
                ESP_LOGW(TAG, "CMD 0x%04x RetrieveCategorizedDatabaseRecords category: 0x%02x not recognised", cmdID, category);
                L0x04::_0x01_iPodAck(esp, iPodAck_BadParam, cmdID);
                break;
            }
        }
        break;

        case L0x04_GetPlayStatus:
        {
            ESP_LOGI(TAG, "CMD 0x%04x GetPlayStatus", cmdID);
            L0x04::_0x1D_ReturnPlayStatus(esp, esp->playPosition, esp->trackDuration, esp->playStatus);
        }
        break;

        case L0x04_GetCurrentPlayingTrackIndex:
        {
            ESP_LOGI(TAG, "CMD 0x%04x GetCurrentPlayingTrackIndex", cmdID);
            L0x04::_0x1F_ReturnCurrentPlayingTrackIndex(esp, esp->currentTrackIndex);
        }
        break;

        case L0x04_GetIndexedPlayingTrackTitle:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackTitle for index %lu (previous %lu)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esp, esp->prevTrackTitle);
            }
            else
            {
                L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esp, esp->trackTitle);
            }
        }
        break;

        case L0x04_GetIndexedPlayingTrackArtistName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackArtistName for index %lu (previous %lu)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esp, esp->prevArtistName);
            }
            else
            {
                L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esp, esp->artistName);
            }
        }
        break;

        case L0x04_GetIndexedPlayingTrackAlbumName:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(TAG, "CMD 0x%04x GetIndexedPlayingTrackAlbumName for index %lu (previous %lu)", cmdID, tempTrackIndex, esp->prevTrackIndex);
            if (tempTrackIndex == esp->prevTrackIndex)
            {
                L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esp, esp->prevAlbumName);
            }
            else
            {
                L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esp, esp->albumName);
            }
        }
        break;

        case L0x04_SetPlayStatusChangeNotification:
        {
            esp->playStatusNotificationState = byteArray[2];
            ESP_LOGI(TAG, "CMD 0x%04x SetPlayStatusChangeNotification 0x%02x", cmdID, esp->playStatusNotificationState);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_PlayCurrentSelection:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(TAG, "CMD 0x%04x PlayCurrentSelection index %lu", cmdID, tempTrackIndex);
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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_PREVIOUS_TRACK);
            }
            else if (tempTrackIndex == esp->currentTrackIndex)
            {
                ESP_LOGD(TAG, "Selected same track as current: %lu", tempTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_NEXT_TRACK);
            }
        }
        break;

        case L0x04_PlayControl:
        {
            ESP_LOGI(TAG, "CMD 0x%04x PlayControl req: 0x%02x vs esp->playStatus: 0x%02x", cmdID, byteArray[2], esp->playStatus);
            switch (byteArray[2])
            {
            case PB_CMD_TOGGLE:
            {
                if (esp->playStatus == PB_STATE_PLAYING)
                {
                    esp->pause();
                }
                else
                {
                    esp->play();
                }
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
            }
            break;
            case PB_CMD_STOP:
            {
                esp->stop();
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
            }
            break;
            case PB_CMD_NEXT_TRACK:
            {
                esp->prevTrackIndex = esp->currentTrackIndex;
                strcpy(esp->prevAlbumName, esp->albumName);
                strcpy(esp->prevArtistName, esp->artistName);
                strcpy(esp->prevTrackTitle, esp->trackTitle);
                esp->prevTrackDuration = esp->trackDuration;

                esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
                esp->currentTrackIndex = esp->trackList[esp->trackListPosition];

                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = get_millis();
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_NEXT_TRACK);
            }
            break;
            case PB_CMD_PREVIOUS_TRACK:
            {
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_PREVIOUS_TRACK);
            }
            break;
            case PB_CMD_NEXT:
            {
                esp->prevTrackIndex = esp->currentTrackIndex;
                strcpy(esp->prevAlbumName, esp->albumName);
                strcpy(esp->prevArtistName, esp->artistName);
                strcpy(esp->prevTrackTitle, esp->trackTitle);
                esp->prevTrackDuration = esp->trackDuration;

                esp->trackListPosition = (esp->trackListPosition + 1) % TOTAL_NUM_TRACKS;
                esp->currentTrackIndex = esp->trackList[esp->trackListPosition];

                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = get_millis();
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_NEXT_TRACK);
            }
            break;
            case PB_CMD_PREV:
            {
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_PREVIOUS_TRACK);
            }
            break;
            case PB_CMD_PLAY:
            {
                esp->play();

                esp->trackChangeAckPending = cmdID;
                esp->trackChangeTimestamp = get_millis();
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);
            }
            break;
            case PB_CMD_PAUSE:
            {
                esp->pause();
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
            }
            break;
            }
            if ((esp->playStatus == PB_STATE_STOPPED) && (esp->playStatusNotificationState == NOTIF_ON))
                L0x04::_0x27_PlayStatusNotification(esp, 0x00);
        }
        break;

        case L0x04_GetShuffle:
        {
            ESP_LOGI(TAG, "CMD 0x%04x GetShuffle", cmdID);
            L0x04::_0x2D_ReturnShuffle(esp, esp->shuffleStatus);
        }
        break;

        case L0x04_SetShuffle:
        {
            ESP_LOGI(TAG, "CMD 0x%04x SetShuffle req: 0x%02x vs shuffleStatus: 0x%02x", cmdID, byteArray[2], esp->shuffleStatus);
            esp->shuffleStatus = byteArray[2];
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetRepeat:
        {
            ESP_LOGI(TAG, "CMD 0x%04x GetRepeat", cmdID);
            L0x04::_0x30_ReturnRepeat(esp, esp->repeatStatus);
        }
        break;

        case L0x04_SetRepeat:
        {
            ESP_LOGI(TAG, "CMD 0x%04x SetRepeat req: 0x%02x vs repeatStatus: 0x%02x", cmdID, byteArray[2], esp->repeatStatus);
            esp->repeatStatus = byteArray[2];
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetMonoDisplayImageLimits:
        {
            ESP_LOGI(TAG, "CMD 0x0%04x GetMonoDisplayImageLimits", cmdID);
            L0x04::_0x34_ReturnMonoDisplayImageLimits(esp, 0, 0, 0x01);
        }
        break;

        case L0x04_GetNumPlayingTracks:
        {
            ESP_LOGI(TAG, "CMD 0x%04x GetNumPlayingTracks", cmdID);
            L0x04::_0x36_ReturnNumPlayingTracks(esp, esp->totalNumberTracks);
        }
        break;

        case L0x04_SetCurrentPlayingTrack:
        {
            tempTrackIndex = swap_endian<uint32_t>(*((uint32_t *)&byteArray[2]));
            ESP_LOGI(TAG, "CMD 0x%04x SetCurrentPlayingTrack index %lu", cmdID, tempTrackIndex);
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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_PREVIOUS_TRACK);
            }
            else if (tempTrackIndex == esp->currentTrackIndex)
            {
                ESP_LOGD(TAG, "Selected same track as current: %lu", tempTrackIndex);
                L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);

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
                L0x04::_0x01_iPodAck(esp, iPodAck_CmdPending, cmdID, TRACK_CHANGE_TIMEOUT);

                if (esp->_playStatusHandler)
                    esp->_playStatusHandler(PB_CMD_NEXT_TRACK);
            }
        }
        break;

        case L0x04_SelectSortDBRecord:
        {
            ESP_LOGI(TAG, "CMD 0x%04x SelectSortDBRecord (deprecated)", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_OK, cmdID);
        }
        break;

        case L0x04_GetColorDisplayImageLimits:
        {
            ESP_LOGI(TAG, "CMD 0x0%04x GetColorDisplayImageLimits", cmdID);
            L0x04::_0x3A_ReturnColorDisplayImageLimits(esp, 0, 0, 0x01);
        }
        break;

        default:
        {
            ESP_LOGW(TAG, "CMD 0x%04x not recognized.", cmdID);
            L0x04::_0x01_iPodAck(esp, iPodAck_CmdFailed, cmdID);
        }
        break;
        }
    }
}

void L0x04::_0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%04x", ackCode, cmdID);
    const uint8_t txPacket[] = {0x04, 0x00, 0x01, (uint8_t)ackCode, 0x00, cmdID};
    if (cmdID == esp->_pendingCmdId_0x04)
    {
        stopTimer(esp->_pendingTimer_0x04);
        esp->_pendingCmdId_0x04 = 0x00;
        esp->_queuePacketToFront(txPacket, sizeof(txPacket));
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID, uint32_t numField)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%04x Numfield: %lu", ackCode, cmdID, numField);
    uint8_t txPacket[20] = {0x04, 0x00, 0x01, (uint8_t)ackCode, cmdID};
    *((uint32_t *)&txPacket[5]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, 9);
    esp->_pendingCmdId_0x04 = cmdID;
    startTimer(esp->_pendingTimer_0x04, numField);
}

void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint8_t trackInfoType, char *trackInfoChars)
{
    ESP_LOGI(TAG, "Req'd track info type: 0x%02x", trackInfoType);
    uint8_t txPacket[255] = {0x04, 0x00, 0x0D, trackInfoType};
    strcpy((char *)&txPacket[4], trackInfoChars);
    esp->_queuePacket(txPacket, 4 + strlen(trackInfoChars) + 1);
}

void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms)
{
    ESP_LOGI(TAG, "Track duration: %lu", trackDuration_ms);
    uint8_t txPacket[14] = {
        0x04, 0x00, 0x0D, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00};
    *((uint32_t *)&txPacket[8]) = swap_endian<uint32_t>(trackDuration_ms);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint8_t trackInfoType, uint16_t releaseYear)
{
    ESP_LOGI(TAG, "Track info: 0x%02x Release Year: %u", trackInfoType, releaseYear);
    uint8_t txPacket[12] = {
        0x04, 0x00, 0x0D, trackInfoType,
        0x00, 0x00, 0x00, 0x01, 0x01,
        0x00, 0x00, 0x01};
    *((uint16_t *)&txPacket[9]) = swap_endian<uint16_t>(releaseYear);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x13_ReturnProtocolVersion(esPod *esp)
{
    ESP_LOGI(TAG, "Lingo protocol version 1.12");
    uint8_t txPacket[] = {0x04, 0x00, 0x13, 0x01, 0x0C};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x19_ReturnNumberCategorizedDBRecords(esPod *esp, uint32_t categoryDBRecords)
{
    ESP_LOGI(TAG, "Category DB Records: %lu", categoryDBRecords);
    uint8_t txPacket[7] = {0x04, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(categoryDBRecords);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x1B_ReturnCategorizedDatabaseRecord(esPod *esp, uint32_t index, char *recordString)
{
    ESP_LOGI(TAG, "Database record at index %lu: %s", index, recordString);
    uint8_t txPacket[255] = {0x04, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(index);
    strcpy((char *)&txPacket[7], recordString);
    esp->_queuePacket(txPacket, 7 + strlen(recordString) + 1);
}

void L0x04::_0x1D_ReturnPlayStatus(esPod *esp, uint32_t position, uint32_t duration, uint8_t playStatusArg)
{
    ESP_LOGI(TAG, "Play status 0x%02x at pos %lu / %lu ms", playStatusArg, position, duration);
    uint8_t txPacket[] = {
        0x04, 0x00, 0x1D,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        playStatusArg};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(duration);
    *((uint32_t *)&txPacket[7]) = swap_endian<uint32_t>(position);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x1F_ReturnCurrentPlayingTrackIndex(esPod *esp, uint32_t trackIndex)
{
    ESP_LOGI(TAG, "Track index: %lu", trackIndex);
    uint8_t txPacket[] = {0x04, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(trackIndex);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x21_ReturnIndexedPlayingTrackTitle(esPod *esp, char *trackTitle)
{
    ESP_LOGI(TAG, "Track title: %s", trackTitle);
    uint8_t txPacket[255] = {0x04, 0x00, 0x21};
    strcpy((char *)&txPacket[3], trackTitle);
    esp->_queuePacket(txPacket, 3 + strlen(trackTitle) + 1);
}

void L0x04::_0x23_ReturnIndexedPlayingTrackArtistName(esPod *esp, char *trackArtistName)
{
    ESP_LOGI(TAG, "Track artist: %s", trackArtistName);
    uint8_t txPacket[255] = {0x04, 0x00, 0x23};
    strcpy((char *)&txPacket[3], trackArtistName);
    esp->_queuePacket(txPacket, 3 + strlen(trackArtistName) + 1);
}

void L0x04::_0x25_ReturnIndexedPlayingTrackAlbumName(esPod *esp, char *trackAlbumName)
{
    ESP_LOGI(TAG, "Track album: %s", trackAlbumName);
    uint8_t txPacket[255] = {0x04, 0x00, 0x25};
    strcpy((char *)&txPacket[3], trackAlbumName);
    esp->_queuePacket(txPacket, 3 + strlen(trackAlbumName) + 1);
}

void L0x04::_0x27_PlayStatusNotification(esPod *esp, uint8_t notification, uint32_t numField)
{
    ESP_LOGI(TAG, "Play status 0x%02x Numfield: %lu", notification, numField);
    uint8_t txPacket[] = {0x04, 0x00, 0x27, notification, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x27_PlayStatusNotification(esPod *esp, uint8_t notification)
{
    ESP_LOGI(TAG, "Play status 0x%02x STOPPED", notification);
    uint8_t txPacket[] = {0x04, 0x00, 0x27, notification};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x2D_ReturnShuffle(esPod *esp, uint8_t currentShuffleStatus)
{
    ESP_LOGI(TAG, "Shuffle status: 0x%02x", currentShuffleStatus);
    uint8_t txPacket[] = {0x04, 0x00, 0x2D, currentShuffleStatus};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x30_ReturnRepeat(esPod *esp, uint8_t currentRepeatStatus)
{
    ESP_LOGI(TAG, "Repeat status: 0x%02x", currentRepeatStatus);
    uint8_t txPacket[] = {0x04, 0x00, 0x30, currentRepeatStatus};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x34_ReturnMonoDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, uint8_t dispPixelFmt)
{
    ESP_LOGI(TAG, "Return monochrome image limits: %u x %u x %u", maxImageW, maxImageH, dispPixelFmt);
    uint8_t txPacket[] = {0x04, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, dispPixelFmt};
    *((uint16_t *)&txPacket[3]) = swap_endian<uint16_t>(maxImageW);
    *((uint16_t *)&txPacket[5]) = swap_endian<uint16_t>(maxImageH);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x36_ReturnNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks)
{
    ESP_LOGI(TAG, "Playing tracks: %lu", numPlayingTracks);
    uint8_t txPacket[] = {0x04, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00};
    *((uint32_t *)&txPacket[3]) = swap_endian<uint32_t>(numPlayingTracks);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

void L0x04::_0x3A_ReturnColorDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, uint8_t dispPixelFmt)
{
    ESP_LOGI(TAG, "Return color image limits: %u x %u x %u", maxImageW, maxImageH, dispPixelFmt);
    uint8_t txPacket[] = {0x04, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x00, dispPixelFmt};
    *((uint16_t *)&txPacket[3]) = swap_endian<uint16_t>(maxImageW);
    *((uint16_t *)&txPacket[5]) = swap_endian<uint16_t>(maxImageH);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}
