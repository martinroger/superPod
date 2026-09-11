#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>

struct MockQueue {
    size_t itemSize;
    std::deque<std::vector<uint8_t>> items;
};

typedef MockQueue * QueueHandle_t;

static inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) {
    auto q = new MockQueue();
    q->itemSize = itemSize;
    return q;
}

static inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t) {
    if (!q) return pdFALSE;
    std::vector<uint8_t> buf(q->itemSize);
    memcpy(buf.data(), item, q->itemSize);
    q->items.push_back(buf);
    return pdTRUE;
}

static inline BaseType_t xQueueSendToFront(QueueHandle_t q, const void *item, TickType_t) {
    if (!q) return pdFALSE;
    std::vector<uint8_t> buf(q->itemSize);
    memcpy(buf.data(), item, q->itemSize);
    q->items.push_front(buf);
    return pdTRUE;
}

static inline BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t) {
    if (!q || q->items.empty()) return pdFALSE;
    memcpy(item, q->items.front().data(), q->itemSize);
    q->items.pop_front();
    return pdTRUE;
}
