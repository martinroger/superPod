/**
 * @file audio_tools_compat.h
 * @brief Non-invasive compatibility header for external components (arduino-audio-tools / ESP32-A2DP)
 *        under ESP-IDF v6.2 (master).
 */

#pragma once

// Pre-include system headers that define HZ macro, then undefine it
#include "freertos/FreeRTOS.h"
#include <sys/param.h>
#include <sys/time.h>

#ifdef HZ
#undef HZ
#endif

#ifdef __cplusplus
#include <algorithm>
#include <utility>

// Provide std::min and std::max in global scope for arduino-audio-tools non-Arduino templates
using std::min;
using std::max;
#endif

// Provide fallback defines for legacy ADC threshold macros removed in ESP-IDF v6.2
#ifndef SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define SOC_ADC_SAMPLE_FREQ_THRES_LOW 20000
#endif

#ifndef SOC_ADC_SAMPLE_FREQ_THRES_HIGH
#define SOC_ADC_SAMPLE_FREQ_THRES_HIGH 2000000
#endif
