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
