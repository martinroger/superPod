/**
 * @file pl2303_usb.cpp
 * @brief TinyUSB Prolific PL2303 Transceiver Emulation Implementation.
 * 
 * =================================================================================
 * Architecture & Design Documentation:
 * =================================================================================
 * This component emulates a Prolific PL2303 (VID 0x067B, PID 0x2303) USB-to-UART
 * transceiver on the native USB-OTG peripheral of the ESP32-S31.
 * 
 * USB Endpoint Mapping:
 *   - Endpoint 0x81 (Interrupt IN): Status IRQ pipe reporting DCD, DSR, CTS, RI line states.
 *   - Endpoint 0x02 (Bulk OUT): Outbound data pipeline from Host to ESP32-S31.
 *   - Endpoint 0x83 (Bulk IN): Inbound data pipeline from ESP32-S31 to Host.
 * 
 * Vendor Control Request Handling:
 *   - Vendor Control Transfer Callback (tud_vendor_control_xfer_cb): Handles PL2303 vendor-specific
 *     setup sequences (requests 0x01, 0x20 SET_LINE, 0x21 GET_LINE, 0x22 SET_CONTROL).
 *   - Line configuration (baudrate, stop bits, parity, data bits) and DTR/RTS bitbanging
 *     control the virtual bridge parameters.
 * 
 * ESP-IDF v6.2 / esp_tinyusb v2.0+ Compatibility:
 *   - Updated to use tinyusb_config_t runtime configuration parameters and event callbacks.
 *   - Explicit PHY configuration (phy.skip_setup = false) and core affinity pinning.
 *   - Updated usbd_edpt_xfer 5-argument signature in TinyUSB 0.21+.
 * =================================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "device/usbd_pvt.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "pl2303_usb.h"

static const char *TAG = "PL2303_USB";

#define DTR_GPIO (gpio_num_t) CONFIG_DTR_PIN
#define RTS_GPIO (gpio_num_t) CONFIG_RTS_PIN
#define BRIDGE_UART_NUM UART_NUM_1

#pragma region USB Descriptors

/// @brief Prolific PL2303 USB Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x067B,  // Prolific Vendor ID
    .idProduct = 0x2303, // PL2303 Product ID
    .bcdDevice = 0x0400,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01
};

/// @brief Prolific PL2303 Configuration Descriptor
uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, (9 + 9 + 7 + 7 + 7), 0x00, 100),

    // Vendor Interface Descriptor
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,

    // Endpoint Interrupt In (Status EP 0x81)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_IRQ, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(10), 0x01,

    // Endpoint Out (Bulk EP 0x02)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_BULK_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

    // Endpoint In (Bulk EP 0x83)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_BULK_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0
};

/// @brief String Descriptors Array
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},              // 0: English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    "USB-Serial Controller",                 // 2: Product
    "",                                      // 3: Serial
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 4: Interface
};

#pragma endregion

#pragma region Globals
static uint8_t line_control = 0; // Line control bitfield byte
#pragma endregion

#pragma region Function Implementations

/// @brief Initializes DTR and RTS control GPIO pins
esp_err_t init_bridge_control_pins(void)
{
    esp_err_t ret;
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << DTR_GPIO) | (1ULL << RTS_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = (esp_err_t)(gpio_set_level(DTR_GPIO, 1) | gpio_set_level(RTS_GPIO, 1));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO set level failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

/// @brief Sends line status to the USB Host over Interrupt Endpoint 0x81
void pl2303_send_status(bool DCD_state, bool CTS_state, bool DSR_state, bool RI_state)
{
    uint8_t status[9] = {0};
    uint8_t final_status = 0;
    final_status |= (DCD_state ? 0x01 : 0x00);
    final_status |= (DSR_state ? 0x02 : 0x00);
    final_status |= (RI_state ? 0x08 : 0x00);
    final_status |= (CTS_state ? 0x80 : 0x00);
    status[8] = final_status;
    if (tud_vendor_mounted())
    {
        tud_vendor_write(status, sizeof(status));
        tud_vendor_write_flush();
    }
    ESP_LOGD(TAG, "Sent PL2303 status: %02X", status[8]);
}

/// @brief Short-hand for sending default active PL2303 status
void pl2303_send_status_active(void)
{
    pl2303_send_status(true, true, true, false);
}

/// @brief TinyUSB Vendor Control Transfer Callback handling PL2303 setup requests
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    static uint8_t req_0404_wIndex = 0x00;
    static uint8_t resp_read_0000 = 0x01;
    static uint8_t set_line_buf[7];

    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
        return false;

    if (stage != CONTROL_STAGE_SETUP)
    {
        if (stage == CONTROL_STAGE_ACK)
        {
            if (request->bRequest == 0x22) // SET CONTROL request
                pl2303_send_status_active();
            if (request->bRequest == 0x20) // SET LINE request
            {
                uint32_t baud = *(uint32_t *)(set_line_buf);
                uart_stop_bits_t stop_bits = (uart_stop_bits_t)(set_line_buf[4] + 1);
                uart_parity_t parity = (set_line_buf[5] == 1 ? UART_PARITY_ODD : (set_line_buf[5] == 2 ? UART_PARITY_EVEN : UART_PARITY_DISABLE));
                uart_word_length_t data_bits = (uart_word_length_t)(set_line_buf[6] - 5);
                if (baud >= 300 && stop_bits >= UART_STOP_BITS_1 && stop_bits <= UART_STOP_BITS_2 && parity >= UART_PARITY_DISABLE && parity <= UART_PARITY_ODD && data_bits >= UART_DATA_5_BITS && data_bits <= UART_DATA_8_BITS)
                {
                    uart_set_baudrate(BRIDGE_UART_NUM, baud);
                    uart_set_stop_bits(BRIDGE_UART_NUM, stop_bits);
                    uart_set_parity(BRIDGE_UART_NUM, parity);
                    uart_set_word_length(BRIDGE_UART_NUM, data_bits);
                }
                pl2303_send_status_active();
            }
        }
        return true;
    }

    if (stage == CONTROL_STAGE_SETUP)
    {
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR && request->bRequest == 0x01)
        {
            if (request->bmRequestType_bit.direction & TUSB_DIR_IN) // Vendor Reads
            {
                static uint8_t resp;
                switch (request->wValue)
                {
                case 0x8484:
                    resp = 0x02;
                    break;
                case 0x8383:
                    resp = 0xEF + req_0404_wIndex;
                    break;
                case 0x8080:
                    resp = 0x01;
                    break;
                case 0x0080:
                    resp = resp_read_0000;
                    break;
                default:
                    resp = 0x00;
                    ESP_LOGD(TAG, "VENDOR READ %04X len %04X", request->wValue, request->wLength);
                    break;
                }
                return tud_control_xfer(rhport, request, &resp, request->wLength);
            }
            else if ((request->bmRequestType_bit.direction == TUSB_DIR_OUT)) // Vendor Writes
            {
                switch (request->wValue)
                {
                case 0x0404:
                    if (request->wIndex == 0x0001)
                        req_0404_wIndex = 0x10;
                    else
                        req_0404_wIndex = 0x00;
                    break;
                case 0x0000:
                    resp_read_0000 = request->wIndex & 0xFF;
                    break;
                default:
                    ESP_LOGD(TAG, "VENDOR WRITE %04X len %04X", request->wValue, request->wLength);
                    break;
                }
            }
            return tud_control_status(rhport, request);
        }

        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
        {
            static uint8_t linebuf[7] = {0, 0, 0, 0, 0, 0, 8};
            static uart_stop_bits_t U_stop_bits;
            static uart_parity_t U_parity;
            static uart_word_length_t U_data_bits;
            static uint32_t U_baudrate;

            switch (request->bRequest)
            {
            case 0x21: // GET LINE
            {
                uart_get_baudrate(BRIDGE_UART_NUM, &U_baudrate);
                uart_get_stop_bits(BRIDGE_UART_NUM, &U_stop_bits);
                uart_get_parity(BRIDGE_UART_NUM, &U_parity);
                uart_get_word_length(BRIDGE_UART_NUM, &U_data_bits);
                linebuf[4] = U_stop_bits - 1;
                linebuf[5] = (U_parity == UART_PARITY_ODD ? 1 : (U_parity == UART_PARITY_EVEN ? 2 : 0));
                linebuf[6] = (U_data_bits == UART_DATA_5_BITS ? 5 : (U_data_bits == UART_DATA_6_BITS ? 6 : (U_data_bits == UART_DATA_7_BITS ? 7 : 8)));
                *((uint32_t *)&linebuf[0]) = U_baudrate;
                return tud_control_xfer(rhport, request, linebuf, 7);
            }
            case 0x20: // SET LINE
            {
                return tud_control_xfer(rhport, request, set_line_buf, 7);
            }
            case 0x22: // SET CONTROL
            {
                line_control = request->wValue & 0xff;
                int dtr = ((line_control & 0x01) != 0) ? 0 : 1;
                int rts = ((line_control & 0x02) != 0) ? 0 : 1;
                gpio_set_level(DTR_GPIO, dtr);
                gpio_set_level(RTS_GPIO, rts);
                return tud_control_status(rhport, request);
            }
            case 0x23: // BREAK
            {
                return tud_control_status(rhport, request);
            }
            default:
                break;
            }
        }
    }
    return false;
}

#ifndef CONFIG_TINYUSB_TASK_PRIORITY
#define CONFIG_TINYUSB_TASK_PRIORITY 15
#endif

/// @brief Initializes the TinyUSB PL2303 Device Driver for ESP-IDF v6.2 (master) / esp_tinyusb v2.0+
esp_err_t pl2303_usb_init(int task_core)
{
    ESP_ERROR_CHECK(init_bridge_control_pins());

    tinyusb_config_t tusb_cfg = {};
    tusb_cfg.port = TINYUSB_PORT_FULL_SPEED_0;
    tusb_cfg.phy.skip_setup = false;
    tusb_cfg.task.size = 4096;
    tusb_cfg.task.priority = CONFIG_TINYUSB_TASK_PRIORITY;
    tusb_cfg.task.xCoreID = task_core;

    tusb_cfg.descriptor.device = &desc_device;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.string_count = 5;
    tusb_cfg.descriptor.full_speed_config = desc_configuration;

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "TinyUSB PL2303 driver initialized successfully on core %d", task_core);
    return ESP_OK;
}

/// @brief Reads incoming USB data from Vendor Bulk OUT endpoint
uint32_t pl2303_usb_read_bytes(uint8_t *buffer, uint32_t bufsize)
{
    if (tud_vendor_available())
    {
        return tud_vendor_read(buffer, bufsize);
    }
    return 0;
}

/// @brief Writes outbound USB data to Vendor Bulk IN endpoint
uint32_t pl2303_usb_write_bytes(const uint8_t *buffer, uint32_t len)
{
    if (tud_vendor_mounted() && !usbd_edpt_busy(0, CONFIG_EP_VENDOR_BULK_IN))
    {
        usbd_edpt_xfer(0, CONFIG_EP_VENDOR_BULK_IN, (uint8_t *)buffer, (uint16_t)len, false);
        return len;
    }
    return 0;
}

#pragma endregion
