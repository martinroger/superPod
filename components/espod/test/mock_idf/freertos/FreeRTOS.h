#pragma once
#include <cstdint>
#include <cstddef>

typedef uint32_t TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFUL
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#include "esp_err.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/ringbuf.h"
