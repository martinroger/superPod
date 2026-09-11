#pragma once
#include "FreeRTOS.h"

typedef void * TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);

static inline TimerHandle_t xTimerCreate(const char *, TickType_t, UBaseType_t, void *, TimerCallbackFunction_t) { return (TimerHandle_t)1; }
static inline BaseType_t xTimerStart(TimerHandle_t, TickType_t) { return pdTRUE; }
static inline BaseType_t xTimerStop(TimerHandle_t, TickType_t) { return pdTRUE; }
static inline BaseType_t xTimerDelete(TimerHandle_t, TickType_t) { return pdTRUE; }
static inline void * pvTimerGetTimerID(TimerHandle_t) { return nullptr; }
static inline BaseType_t xTimerIsTimerActive(TimerHandle_t) { return pdFALSE; }
static inline BaseType_t xTimerChangePeriod(TimerHandle_t, TickType_t, TickType_t) { return pdTRUE; }
