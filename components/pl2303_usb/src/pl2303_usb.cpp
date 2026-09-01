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

#ifndef CONFIG_DTR_PIN
#define CONFIG_DTR_PIN -1
#endif

#ifndef CONFIG_RTS_PIN
#define CONFIG_RTS_PIN -1
#endif

#define DTR_GPIO (gpio_num_t) CONFIG_DTR_PIN
#define RTS_GPIO (gpio_num_t) CONFIG_RTS_PIN
#define BRIDGE_UART_NUM UART_NUM_1

#ifndef CONFIG_PL2303_DEFAULT_BAUDRATE
#define CONFIG_PL2303_DEFAULT_BAUDRATE 19200
#endif

#pragma region USB Descriptors

/// @brief Prolific PL2303 USB Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
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

/// @brief Device Qualifier Descriptor (required for High-Speed capable USB devices)
static const tusb_desc_device_qualifier_t desc_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00
};

/// @brief Prolific PL2303 Full-Speed Configuration Descriptor
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

/// @brief Prolific PL2303 High-Speed Configuration Descriptor
uint8_t const desc_hs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, (9 + 9 + 7 + 7 + 7), 0x00, 100),

    // Vendor Interface Descriptor
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,

    // Endpoint Interrupt In (Status EP 0x81)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_IRQ, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(10), 0x01,

    // Endpoint Out (Bulk EP 0x02, 512 bytes for High-Speed)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_BULK_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,

    // Endpoint In (Bulk EP 0x83, 512 bytes for High-Speed)
    7, TUSB_DESC_ENDPOINT, CONFIG_EP_VENDOR_BULK_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0
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
static pl2303_line_coding_t s_line_coding = {
    .baud_rate = CONFIG_PL2303_DEFAULT_BAUDRATE,
    .stop_bits = 0, // 0: 1 stop bit
    .parity = 0,    // 0: None
    .data_bits = 8  // 8 data bits
};
static bool s_dtr_state = false;
static bool s_rts_state = false;
static pl2303_line_coding_cb_t s_line_coding_cb = nullptr;
#pragma endregion

#pragma region Function Implementations

/// @brief Initializes optional DTR and RTS control GPIO pins (skipped if pins < 0)
esp_err_t init_bridge_control_pins(void)
{
    if ((int)CONFIG_DTR_PIN < 0 && (int)CONFIG_RTS_PIN < 0)
    {
        ESP_LOGI(TAG, "Hardware control pins disabled (pure virtual mode)");
        return ESP_OK;
    }

    uint64_t pin_mask = 0;
    if ((int)CONFIG_DTR_PIN >= 0)
    {
        pin_mask |= (1ULL << DTR_GPIO);
    }
    if ((int)CONFIG_RTS_PIN >= 0)
    {
        pin_mask |= (1ULL << RTS_GPIO);
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = pin_mask;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if ((int)CONFIG_DTR_PIN >= 0)
    {
        gpio_set_level(DTR_GPIO, 1);
    }
    if ((int)CONFIG_RTS_PIN >= 0)
    {
        gpio_set_level(RTS_GPIO, 1);
    }
    ESP_LOGI(TAG, "Hardware control pins configured: DTR=%d, RTS=%d", (int)CONFIG_DTR_PIN, (int)CONFIG_RTS_PIN);
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
            {
                pl2303_send_status_active();
            }
            if (request->bRequest == 0x20) // SET LINE request
            {
                uint32_t baud = (uint32_t)set_line_buf[0] |
                                ((uint32_t)set_line_buf[1] << 8) |
                                ((uint32_t)set_line_buf[2] << 16) |
                                ((uint32_t)set_line_buf[3] << 24);
                uint8_t stop_bits = set_line_buf[4];
                uint8_t parity = set_line_buf[5];
                uint8_t data_bits = set_line_buf[6];

                // Host-adaptive: adopt whatever baudrate and framing the host requested
                s_line_coding.baud_rate = baud;
                s_line_coding.stop_bits = stop_bits;
                s_line_coding.parity = parity;
                s_line_coding.data_bits = data_bits;

                ESP_LOGI(TAG, "Host SET_LINE: baud=%lu, stop_bits=%u, parity=%u, data_bits=%u",
                         (unsigned long)baud, stop_bits, parity, data_bits);

                if (s_line_coding_cb != nullptr)
                {
                    s_line_coding_cb(&s_line_coding);
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
            static uint8_t linebuf[7];

            switch (request->bRequest)
            {
            case 0x21: // GET LINE
            {
                linebuf[0] = (uint8_t)(s_line_coding.baud_rate & 0xFF);
                linebuf[1] = (uint8_t)((s_line_coding.baud_rate >> 8) & 0xFF);
                linebuf[2] = (uint8_t)((s_line_coding.baud_rate >> 16) & 0xFF);
                linebuf[3] = (uint8_t)((s_line_coding.baud_rate >> 24) & 0xFF);
                linebuf[4] = s_line_coding.stop_bits;
                linebuf[5] = s_line_coding.parity;
                linebuf[6] = s_line_coding.data_bits;

                ESP_LOGD(TAG, "Host GET_LINE: returning baud=%lu, stop_bits=%u, parity=%u, data_bits=%u",
                         (unsigned long)s_line_coding.baud_rate, s_line_coding.stop_bits, s_line_coding.parity, s_line_coding.data_bits);

                return tud_control_xfer(rhport, request, linebuf, 7);
            }
            case 0x20: // SET LINE
            {
                return tud_control_xfer(rhport, request, set_line_buf, 7);
            }
            case 0x22: // SET CONTROL
            {
                line_control = request->wValue & 0xff;
                s_dtr_state = ((line_control & 0x01) != 0);
                s_rts_state = ((line_control & 0x02) != 0);

                int dtr_level = s_dtr_state ? 0 : 1;
                int rts_level = s_rts_state ? 0 : 1;

                if ((int)CONFIG_DTR_PIN >= 0)
                {
                    gpio_set_level(DTR_GPIO, dtr_level);
                }
                if ((int)CONFIG_RTS_PIN >= 0)
                {
                    gpio_set_level(RTS_GPIO, rts_level);
                }

                ESP_LOGD(TAG, "Host SET_CONTROL: DTR=%d, RTS=%d (GPIO DTR=%d, RTS=%d)",
                         s_dtr_state, s_rts_state, dtr_level, rts_level);
                return tud_control_status(rhport, request);
            }
            case 0x23: // BREAK
            {
                ESP_LOGD(TAG, "Host BREAK request: value=0x%04X", request->wValue);
                return tud_control_status(rhport, request);
            }
            default:
                break;
            }
        }
    }
    return false;
}

/// @brief Injects or sets virtual UART line coding parameters
esp_err_t pl2303_usb_set_line_coding(const pl2303_line_coding_t *coding)
{
    if (coding == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    s_line_coding = *coding;
    ESP_LOGI(TAG, "Virtual line coding injected: baud=%lu, stop_bits=%u, parity=%u, data_bits=%u",
             (unsigned long)s_line_coding.baud_rate, s_line_coding.stop_bits, s_line_coding.parity, s_line_coding.data_bits);
    return ESP_OK;
}

/// @brief Retrieves current virtual UART line coding parameters
esp_err_t pl2303_usb_get_line_coding(pl2303_line_coding_t *coding)
{
    if (coding == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    *coding = s_line_coding;
    return ESP_OK;
}

/// @brief Registers a callback to be notified when USB Host updates line coding via SET_LINE
void pl2303_usb_set_line_coding_callback(pl2303_line_coding_cb_t cb)
{
    s_line_coding_cb = cb;
}

/// @brief Retrieves current virtual control line state (DTR / RTS) set by USB host
void pl2303_usb_get_control_lines(bool *dtr, bool *rts)
{
    if (dtr) *dtr = s_dtr_state;
    if (rts) *rts = s_rts_state;
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
    tusb_cfg.descriptor.qualifier = &desc_qualifier;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.string_count = 5;
    tusb_cfg.descriptor.full_speed_config = desc_configuration;
    tusb_cfg.descriptor.high_speed_config = desc_hs_configuration;

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "TinyUSB PL2303 driver initialized successfully on core %d", task_core);
    return ESP_OK;
}

static TaskHandle_t s_rx_task_handle = NULL;

/// @brief Registers task handle to receive FreeRTOS task notifications when TinyUSB receives Bulk OUT data
void pl2303_usb_set_rx_task_handle(TaskHandle_t task_handle)
{
    s_rx_task_handle = task_handle;
}

/// @brief TinyUSB Vendor RX callback invoked when Bulk OUT data arrives from host
extern "C" void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint16_t bufsize)
{
    (void)idx;
    (void)buffer;
    (void)bufsize;
    if (s_rx_task_handle != NULL)
    {
        xTaskNotifyGive(s_rx_task_handle);
    }
}

/// @brief Reads incoming USB data from Vendor Bulk OUT endpoint
uint32_t pl2303_usb_read_bytes(uint8_t *buffer, uint32_t bufsize)
{
    if (tud_vendor_available())
    {
        return tud_vendor_read(buffer, bufsize);
        uint32_t read_len = tud_vendor_read(buffer, bufsize);
        if (read_len > 0)
        {
            ESP_LOGD(TAG, "Bulk OUT read: %lu bytes", (unsigned long)read_len);
        }
        return read_len;
    }
    return 0;
}

/// @brief Writes outbound USB data to Vendor Bulk IN endpoint
uint32_t pl2303_usb_write_bytes(const uint8_t *buffer, uint32_t len)
{
    if (tud_vendor_mounted() && !usbd_edpt_busy(0, CONFIG_EP_VENDOR_BULK_IN))
    {
        usbd_edpt_xfer(0, CONFIG_EP_VENDOR_BULK_IN, (uint8_t *)buffer, (uint16_t)len, false);
        ESP_LOGD(TAG, "Bulk IN scheduled: %lu bytes", (unsigned long)len);
        return len;
    }
    return 0;
}

#pragma endregion
