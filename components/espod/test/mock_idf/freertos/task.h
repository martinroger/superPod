#pragma once
#include "FreeRTOS.h"

typedef void * TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

static inline BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *) { return pdTRUE; }
static inline void vTaskDelete(TaskHandle_t) {}
static inline void vTaskDelay(TickType_t) {}
static inline uint32_t ulTaskNotifyTake(BaseType_t, TickType_t) { return 0; }
static inline BaseType_t xTaskNotifyGive(TaskHandle_t) { return pdTRUE; }
