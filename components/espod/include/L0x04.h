/**
 * @file L0x04.h
 * @brief Extended Interface Lingo 0x04 Processor Header for esPod.
 */

#pragma once

#include <cstdint>
#include "esPod_utils.h"

class esPod;

#define L0x04_GetIndexedPlayingTrackInfo 0x0C
#define L0x04_RequestProtocolVersion 0x12
#define L0x04_ResetDBSelection 0x16
#define L0x04_SelectDBRecord 0x17
#define L0x04_GetNumberCategorizedDBRecords 0x18
#define L0x04_RetrieveCategorizedDatabaseRecords 0x1A
#define L0x04_GetPlayStatus 0x1C
#define L0x04_GetCurrentPlayingTrackIndex 0x1E
#define L0x04_GetIndexedPlayingTrackTitle 0x20
#define L0x04_GetIndexedPlayingTrackArtistName 0x22
#define L0x04_GetIndexedPlayingTrackAlbumName 0x24
#define L0x04_SetPlayStatusChangeNotification 0x26
#define L0x04_PlayCurrentSelection 0x28
#define L0x04_PlayControl 0x29
#define L0x04_GetShuffle 0x2C
#define L0x04_SetShuffle 0x2E
#define L0x04_GetRepeat 0x2F
#define L0x04_SetRepeat 0x31
#define L0x04_GetMonoDisplayImageLimits 0x33
#define L0x04_GetNumPlayingTracks 0x35
#define L0x04_SetCurrentPlayingTrack 0x37
#define L0x04_SelectSortDBRecord 0x38
#define L0x04_GetColorDisplayImageLimits 0x39

/**
 * @brief Processor class for Extended Interface Lingo (0x04) commands.
 */
class L0x04
{
public:
    static void processLingo(esPod *esp, const uint8_t *byteArray, uint32_t len);

    static void _0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID);
    static void _0x01_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID, uint32_t numField);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint8_t trackInfoType, char *trackInfoChars);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint32_t trackDuration_ms);
    static void _0x0D_ReturnIndexedPlayingTrackInfo(esPod *esp, uint8_t trackInfoType, uint16_t releaseYear);
    static void _0x13_ReturnProtocolVersion(esPod *esp);
    static void _0x19_ReturnNumberCategorizedDBRecords(esPod *esp, uint32_t categoryDBRecords);
    static void _0x1B_ReturnCategorizedDatabaseRecord(esPod *esp, uint32_t index, char *recordString);
    static void _0x1D_ReturnPlayStatus(esPod *esp, uint32_t position, uint32_t duration, uint8_t playStatus);
    static void _0x1F_ReturnCurrentPlayingTrackIndex(esPod *esp, uint32_t trackIndex);
    static void _0x21_ReturnIndexedPlayingTrackTitle(esPod *esp, char *trackTitle);
    static void _0x23_ReturnIndexedPlayingTrackArtistName(esPod *esp, char *trackArtistName);
    static void _0x25_ReturnIndexedPlayingTrackAlbumName(esPod *esp, char *trackAlbumName);
    static void _0x27_PlayStatusNotification(esPod *esp, uint8_t notification, uint32_t numField);
    static void _0x27_PlayStatusNotification(esPod *esp, uint8_t notification);
    static void _0x2D_ReturnShuffle(esPod *esp, uint8_t shuffleStatus);
    static void _0x30_ReturnRepeat(esPod *esp, uint8_t repeatStatus);
    static void _0x34_ReturnMonoDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, uint8_t dispPixelFmt);
    static void _0x36_ReturnNumPlayingTracks(esPod *esp, uint32_t numPlayingTracks);
    static void _0x3A_ReturnColorDisplayImageLimits(esPod *esp, uint16_t maxImageW, uint16_t maxImageH, uint8_t dispPixelFmt);
};
