#pragma once
#include <deque>
#include <vector>
#include <cstring>

typedef enum {
    RINGBUF_TYPE_NOSPLIT = 0,
    RINGBUF_TYPE_ALLOWSPLIT,
    RINGBUF_TYPE_BYTEBUF
} RingbufferType_t;

struct MockRingbuf {
    std::deque<std::vector<uint8_t>> packets;
};

typedef MockRingbuf * RingbufHandle_t;

static inline RingbufHandle_t xRingbufferCreate(size_t, RingbufferType_t) {
    return new MockRingbuf();
}

static inline BaseType_t xRingbufferSend(RingbufHandle_t rb, const void *data, size_t len, TickType_t) {
    if (!rb) return pdFALSE;
    std::vector<uint8_t> pkt((const uint8_t *)data, (const uint8_t *)data + len);
    rb->packets.push_back(pkt);
    return pdTRUE;
}

static inline void * xRingbufferReceive(RingbufHandle_t rb, size_t *itemSize, TickType_t) {
    if (!rb || rb->packets.empty()) {
        if (itemSize) *itemSize = 0;
        return nullptr;
    }
    static std::vector<uint8_t> current;
    current = rb->packets.front();
    rb->packets.pop_front();
    if (itemSize) *itemSize = current.size();
    return current.data();
}

static inline void vRingbufferReturnItem(RingbufHandle_t, void *) {}

static inline void vRingbufferDelete(RingbufHandle_t rb) {
    delete rb;
}
