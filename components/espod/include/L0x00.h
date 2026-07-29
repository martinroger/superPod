/**
 * @file L0x00.h
 * @brief General Lingo 0x00 Processor Header for esPod.
 */

#pragma once

#include <cstdint>
#include "esPod_utils.h"

class esPod;

#define L0x00_Identify 0x01
#define L0x00_RequestExtendedInterfaceMode 0x03
#define L0x00_EnterExtendedInterfaceMode 0x05
#define L0x00_ExitExtendedInterfaceMode 0x06
#define L0x00_RequestiPodName 0x07
#define L0x00_RequestiPodSoftwareVersion 0x09
#define L0x00_RequestiPodSerialNum 0x0B
#define L0x00_RequestiPodModelNum 0x0D
#define L0x00_RequestLingoProtocolVersion 0x0F
#define L0x00_IdentifyDeviceLingoes 0x13
#define L0x00_GetiPodOptions 0x24
#define L0x00_RetAccessoryInfo 0x28

/**
 * @brief Processor class for General Lingo (0x00) commands.
 */
class L0x00
{
public:
    /**
     * @brief Parses and dispatches incoming Lingo 0x00 commands.
     * 
     * @param esp Pointer to parent esPod instance.
     * @param byteArray Pointer to command packet starting at Command ID byte.
     * @param len Packet length.
     */
    static void processLingo(esPod *esp, const uint8_t *byteArray, uint32_t len);

    static void _0x00_RequestIdentify(esPod *esp);
    static void _0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID);
    static void _0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID, uint32_t numField);
    static void _0x04_ReturnExtendedInterfaceMode(esPod *esp, uint8_t extendedModeByte);
    static void _0x08_ReturniPodName(esPod *esp);
    static void _0x0A_ReturniPodSoftwareVersion(esPod *esp);
    static void _0x0C_ReturniPodSerialNum(esPod *esp);
    static void _0x0E_ReturniPodModelNum(esPod *esp);
    static void _0x10_ReturnLingoProtocolVersion(esPod *esp, uint8_t targetLingo);
    static void _0x25_RetiPodOptions(esPod *esp, uint64_t optBitField);
    static void _0x27_GetAccessoryInfo(esPod *esp, uint8_t desiredInfo);
};
