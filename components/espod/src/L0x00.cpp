/**
 * @file L0x00.cpp
 * @brief General Lingo 0x00 Command Implementation.
 */

#include "L0x00.h"
#include "esPod.h"
#include "esp_log.h"

static const char *TAG = "L0x00";

/// @brief Parses and dispatches incoming Lingo 0x00 commands
void L0x00::processLingo(esPod *esp, const uint8_t *byteArray, uint32_t len)
{
    uint8_t cmdID = byteArray[0];
    uint64_t iPodOptions = 0;

    switch (cmdID)
    {
    case L0x00_Identify:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x Identify with Lingo 0x%02x", cmdID, byteArray[1]);
    }
    break;

    case L0x00_RequestExtendedInterfaceMode:
    {
        ESP_LOGD(TAG, "CMD: 0x%02x RequestExtendedInterfaceMode", cmdID);
        L0x00::_0x04_ReturnExtendedInterfaceMode(esp, esp->extendedInterfaceModeActive ? 0x01 : 0x00);
    }
    break;

    case L0x00_EnterExtendedInterfaceMode:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x EnterExtendedInterfaceMode", cmdID);
        esp->extendedInterfaceModeActive = true;
        L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
    }
    break;

    case L0x00_ExitExtendedInterfaceMode:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x ExitExtendedInterfaceMode", cmdID);
        if (esp->extendedInterfaceModeActive)
        {
            L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
            esp->extendedInterfaceModeActive = false;
            esp->playStatusNotificationState = NOTIF_OFF;
        }
        else
        {
            L0x00::_0x02_iPodAck(esp, iPodAck_BadParam, cmdID);
        }
    }
    break;

    case L0x00_RequestiPodName:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RequestiPodName", cmdID);
        L0x00::_0x08_ReturniPodName(esp);
    }
    break;

    case L0x00_RequestiPodSoftwareVersion:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RequestiPodSoftwareVersion", cmdID);
        L0x00::_0x0A_ReturniPodSoftwareVersion(esp);
    }
    break;

    case L0x00_RequestiPodSerialNum:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RequestiPodSerialNum", cmdID);
        L0x00::_0x0C_ReturniPodSerialNum(esp);
    }
    break;

    case L0x00_RequestiPodModelNum:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RequestiPodModelNum", cmdID);
        L0x00::_0x0E_ReturniPodModelNum(esp);
    }
    break;

    case L0x00_RequestLingoProtocolVersion:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RequestLingoProtocolVersion for Lingo 0x%02x", cmdID, byteArray[1]);
        L0x00::_0x10_ReturnLingoProtocolVersion(esp, byteArray[1]);
    }
    break;

    case L0x00_IdentifyDeviceLingoes:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x IdentifyDeviceLingoes : L 0x%02x - Opt 0x%02x - ID 0x%02x", cmdID, byteArray[1], byteArray[2], byteArray[3]);
        L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x00);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x01);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x04);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x05);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x06);
        L0x00::_0x27_GetAccessoryInfo(esp, 0x07);
    }
    break;

    case L0x00_GetiPodOptions:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x GetiPodOptions", cmdID);
        L0x00::_0x25_RetiPodOptions(esp, iPodOptions);
    }
    break;

    case L0x00_RetAccessoryInfo:
    {
        ESP_LOGI(TAG, "CMD: 0x%02x RetAccessoryInfo: 0x%02x", cmdID, byteArray[1]);
        switch (byteArray[1])
        {
        case 0x00:
            ESP_LOGI(TAG, "\tAccessory Capabilities : 0x%02x", byteArray[2]);
            break;
        case 0x01:
            ESP_LOGI(TAG, "\tAccessory Name : %s", &byteArray[2]);
            break;
        case 0x04:
            ESP_LOGI(TAG, "\tAccessory Firmware : %d.%d.%d", byteArray[2], byteArray[3], byteArray[4]);
            break;
        case 0x05:
            ESP_LOGI(TAG, "\tAccessory Hardware : %d.%d.%d", byteArray[2], byteArray[3], byteArray[4]);
            break;
        case 0x06:
            ESP_LOGI(TAG, "\tAccessory Manufacturer : %s", &byteArray[2]);
            break;
        case 0x07:
            ESP_LOGI(TAG, "\tAccessory Model : %s", &byteArray[2]);
            break;
        default:
            L0x00::_0x02_iPodAck(esp, iPodAck_OK, cmdID);
            break;
        }
    }
    break;

    default:
    {
        ESP_LOGW(TAG, "CMD 0x%02x not recognized.", cmdID);
        L0x00::_0x02_iPodAck(esp, iPodAck_CmdFailed, cmdID);
    }
    break;
    }
}

/// @brief Requests Accessory Identify restart
void L0x00::_0x00_RequestIdentify(esPod *esp)
{
    ESP_LOGI(TAG, "iPod: RequestIdentify");
    const uint8_t txPacket[] = {0x00, 0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Sends general response command for Lingo 0x00
void L0x00::_0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%02x", ackCode, cmdID);
    const uint8_t txPacket[] = {0x00, 0x02, (uint8_t)ackCode, cmdID};
    if (cmdID == esp->_pendingCmdId_0x00)
    {
        stopTimer(esp->_pendingTimer_0x00);
        esp->_pendingCmdId_0x00 = 0x00;
        esp->_queuePacketToFront(txPacket, sizeof(txPacket));
    }
    else
        esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Sends general response command with numerical pending delay
void L0x00::_0x02_iPodAck(esPod *esp, IPOD_ACK_CODE ackCode, uint8_t cmdID, uint32_t numField)
{
    ESP_LOGI(TAG, "Ack 0x%02x to command 0x%02x Numfield: %lu", ackCode, cmdID, numField);
    uint8_t txPacket[20] = {0x00, 0x02, (uint8_t)ackCode, cmdID};
    *((uint32_t *)&txPacket[4]) = swap_endian<uint32_t>(numField);
    esp->_queuePacket(txPacket, 8);
    esp->_pendingCmdId_0x00 = cmdID;
    startTimer(esp->_pendingTimer_0x00, numField);
}

/// @brief Returns extended interface mode status byte
void L0x00::_0x04_ReturnExtendedInterfaceMode(esPod *esp, uint8_t extendedModeByte)
{
    ESP_LOGD(TAG, "Extended Interface mode: 0x%02x", extendedModeByte);
    const uint8_t txPacket[] = {0x00, 0x04, extendedModeByte};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns iPod device name string
void L0x00::_0x08_ReturniPodName(esPod *esp)
{
    ESP_LOGI(TAG, "Name: %s", esp->_name);
    uint8_t txPacket[255] = {0x00, 0x08};
    strcpy((char *)&txPacket[2], esp->_name);
    esp->_queuePacket(txPacket, 3 + strlen(esp->_name));
}

/// @brief Returns iPod software version
void L0x00::_0x0A_ReturniPodSoftwareVersion(esPod *esp)
{
    ESP_LOGI(TAG, "SW version: %d.%d.%d", esp->_SWMajor, esp->_SWMinor, esp->_SWrevision);
    uint8_t txPacket[] = {0x00, 0x0A, esp->_SWMajor, esp->_SWMinor, esp->_SWrevision};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns iPod serial number string
void L0x00::_0x0C_ReturniPodSerialNum(esPod *esp)
{
    ESP_LOGI(TAG, "Serial number: %s", esp->_serialNumber);
    uint8_t txPacket[255] = {0x00, 0x0C};
    strcpy((char *)&txPacket[2], esp->_serialNumber);
    esp->_queuePacket(txPacket, 3 + strlen(esp->_serialNumber));
}

/// @brief Returns iPod model number string
void L0x00::_0x0E_ReturniPodModelNum(esPod *esp)
{
    ESP_LOGI(TAG, "Model number: PA146FD 720901");
    uint8_t txPacket[] = {0x00, 0x0E, 0x00, 0x0B, 0x00, 0x05, 0x50, 0x41, 0x31, 0x34, 0x36, 0x46, 0x44, 0x00};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns protocol version for target Lingo
void L0x00::_0x10_ReturnLingoProtocolVersion(esPod *esp, uint8_t targetLingo)
{
    uint8_t txPacket[] = {0x00, 0x10, targetLingo, 0x01, 0x00};
    switch (targetLingo)
    {
    case 0x00:
        txPacket[4] = 0x06;
        break;
    case 0x03:
        txPacket[4] = 0x05;
        break;
    case 0x04:
        txPacket[4] = 0x0C;
        break;
    case 0x0A:
        txPacket[4] = 0x00;
        break;
    }
    ESP_LOGI(TAG, "Lingo 0x%02x protocol version: 1.%d", targetLingo, txPacket[4]);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Returns iPod options bitfield
void L0x00::_0x25_RetiPodOptions(esPod *esp, uint64_t optBitField)
{
    ESP_LOGI(TAG, "Returning iPod Options");
    uint8_t txPacket[] = {0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    *((uint64_t *)&txPacket[2]) = swap_endian<uint64_t>(optBitField);
    esp->_queuePacket(txPacket, sizeof(txPacket));
}

/// @brief Queries accessory information category
void L0x00::_0x27_GetAccessoryInfo(esPod *esp, uint8_t desiredInfo)
{
    ESP_LOGI(TAG, "Req'd info type: 0x%02x", desiredInfo);
    uint8_t txPacket[] = {0x00, 0x27, desiredInfo};
    esp->_queuePacket(txPacket, sizeof(txPacket));
}
