#pragma once
#include <cstdint>
#include <cstdbool>
#include "esp_err.h"

typedef int uart_port_t;
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_NUM_MAX 2

typedef enum {
    UART_DATA_8_BITS = 0
} uart_word_length_t;

typedef enum {
    UART_PARITY_DISABLE = 0
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 0
} uart_stop_bits_t;

typedef enum {
    UART_HW_FLOWCTRL_DISABLE = 0
} uart_hw_flowcontrol_t;

typedef enum {
    UART_SCLK_DEFAULT = 0
} uart_sclk_t;

typedef struct {
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uart_sclk_t source_clk;
} uart_config_t;

static inline bool uart_is_driver_installed(uart_port_t) { return false; }
static inline esp_err_t uart_driver_install(uart_port_t, int, int, int, void*, int) { return ESP_OK; }
static inline esp_err_t uart_param_config(uart_port_t, const uart_config_t *) { return ESP_OK; }
static inline esp_err_t uart_set_pin(uart_port_t, int, int, int, int) { return ESP_OK; }
static inline int uart_read_bytes(uart_port_t, void *, uint32_t, uint32_t) { return 0; }
static inline int uart_write_bytes(uart_port_t, const void *, size_t) { return 0; }
static inline void uart_driver_delete(uart_port_t) {}
