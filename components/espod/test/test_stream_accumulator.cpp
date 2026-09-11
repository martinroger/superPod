/**
 * @file test_stream_accumulator.cpp
 * @brief Real-code regression test suite for esPod byte-stream packet accumulator.
 *
 * Directly tests the production component sources in components/espod/src/esPod.cpp.
 * Zero hardcoded copies or mock duplicates of the accumulator algorithm.
 * Compatible with Ubuntu (Linux x86_64 / aarch64) and macOS (Darwin x86_64 / Apple Silicon).
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>

#include "esPod.h"

static int totalTests = 0;
static int passedTests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "\n  [FAIL] Assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        totalTests++; \
        printf("Running %-55s ... ", #fn); \
        fflush(stdout); \
        fn(); \
        passedTests++; \
        printf("[PASS]\n"); \
    } while (0)

// Helper to pop all queued packets from the real esPod command ringbuffer
static std::vector<std::vector<uint8_t>> drainRingBuffer(esPod &esp) {
    std::vector<std::vector<uint8_t>> pkts;
    size_t itemSize = 0;
    while (void *item = xRingbufferReceive(esp.getCmdRingBuffer(), &itemSize, 0)) {
        if (item && itemSize > 0) {
            std::vector<uint8_t> pkt((uint8_t *)item, (uint8_t *)item + itemSize);
            pkts.push_back(pkt);
        }
    }
    return pkts;
}

// Test 1: 19-byte Mini Cooper trace packet fed 1 byte at a time to real esPod
void test_mini_cooper_1byte_stream() {
    esPod esp(1, -1, -1, 19200);

    // Trace: FF FF 55 0E 00 13 00 00 00 01 00 00 00 00 00 00 00 00 DE
    uint8_t traceData[] = {
        0xFF, 0xFF, 0x55, 0x0E, 0x00, 0x13, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDE
    };

    for (size_t i = 0; i < sizeof(traceData); i++) {
        esp.processRawBuffer(&traceData[i], 1);
        if (i >= 2 && i < sizeof(traceData) - 1) {
            TEST_ASSERT(esp.isRxIncomplete(), "Should be mid-packet during trace ingestion");
        }
    }

    TEST_ASSERT(!esp.isRxIncomplete(), "Accumulator should be idle after full packet");
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 1, "Expected exactly 1 queued packet");
    TEST_ASSERT(pkts[0].size() == 18, "Assembled iAP frame size should be 18 bytes");
    TEST_ASSERT(pkts[0][0] == 0xFF && pkts[0][1] == 0x55, "Header match");
    TEST_ASSERT(pkts[0][2] == 0x0E, "Length match");
    TEST_ASSERT(pkts[0][17] == 0xDE, "Checksum match");
}

// Test 2: Multi-packet contiguous buffer feed to real esPod
void test_multi_packet_contiguous() {
    esPod esp(1, -1, -1, 19200);
    uint8_t multiData[] = {
        0x00, 0x12, // Leading noise
        0xFF, 0x55, 0x02, 0x00, 0x01, 0xFD, // Pkt 1: (sum = 2+0+1=3, 256-3 = 0xFD)
        0xFF, 0xFF, 0xFF, 0x55, 0x02, 0x00, 0x02, 0xFC  // Pkt 2: (sum = 2+0+2=4, 256-4 = 0xFC)
    };

    esp.processRawBuffer(multiData, sizeof(multiData));
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 2, "Expected 2 queued packets");
    TEST_ASSERT(pkts[0].size() == 6, "Pkt 1 length");
    TEST_ASSERT(pkts[1].size() == 6, "Pkt 2 length");
    TEST_ASSERT(pkts[1][5] == 0xFC, "Pkt 2 checksum match");
}

// Test 3: Corrupted checksum rejection
void test_bad_checksum_rejection() {
    esPod esp(1, -1, -1, 19200);
    uint8_t badData[] = { 0xFF, 0x55, 0x02, 0x00, 0x01, 0x00 }; // 0x00 != 0xFD
    esp.processRawBuffer(badData, sizeof(badData));
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.empty(), "Corrupted packet should not be queued");
    TEST_ASSERT(!esp.isRxIncomplete(), "Accumulator should reset after bad checksum");
}

// Test 4: Incomplete packet timeout reset cleanly recovers
void test_timeout_reset_recovery() {
    esPod esp(1, -1, -1, 19200);
    uint8_t partial[] = { 0xFF, 0x55, 0x0E, 0x00, 0x13 };
    esp.processRawBuffer(partial, sizeof(partial));
    TEST_ASSERT(esp.isRxIncomplete(), "Should be incomplete");
    TEST_ASSERT(esp.getAccumulatorCursor() == 5, "Cursor should be at 5");

    // Simulate 500 ms inter-byte timeout
    esp.resetAccumulator();
    TEST_ASSERT(!esp.isRxIncomplete(), "Incomplete flag cleared after reset");
    TEST_ASSERT(esp.getAccumulatorCursor() == 0, "Cursor cleared after reset");

    // Arriving clean packet should assemble normally
    uint8_t cleanPkt[] = { 0xFF, 0x55, 0x02, 0x00, 0x01, 0xFD };
    esp.processRawBuffer(cleanPkt, sizeof(cleanPkt));
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 1, "Clean packet queued after timeout reset");
}

// Test 5: Invalid payload length byte (0 is rejected)
void test_invalid_length_byte() {
    esPod esp(1, -1, -1, 19200);
    uint8_t zeroLen[] = { 0xFF, 0x55, 0x00 };
    esp.processRawBuffer(zeroLen, sizeof(zeroLen));
    TEST_ASSERT(!esp.isRxIncomplete(), "Zero length rejected");
    auto pkts1 = drainRingBuffer(esp);
    TEST_ASSERT(pkts1.empty(), "No packet queued for length 0");

    uint8_t validPkt[] = { 0xFF, 0x55, 0x02, 0x00, 0x01, 0xFD };
    esp.processRawBuffer(validPkt, sizeof(validPkt));
    auto pkts2 = drainRingBuffer(esp);
    TEST_ASSERT(pkts2.size() == 1, "Subsequent packet queued");
}

// Test 6: USER REQUIREMENT - Mixed consolidated data streams (bursts)
// Burst 1: Complete packet A + first half of packet B (one-and-a-half packets)
// Burst 2: Second half of packet B + complete packet C
void test_mixed_consolidated_data_streams_burst() {
    esPod esp(1, -1, -1, 19200);

    // Burst 1: Packet A (6B) + First 9 bytes of Packet B (FF 55 0E 00 13 00 00 00 01)
    uint8_t burst1[] = {
        0xFF, 0x55, 0x02, 0x00, 0x01, 0xFD,                         // Complete Packet A
        0xFF, 0x55, 0x0E, 0x00, 0x13, 0x00, 0x00, 0x00, 0x01       // First 9 bytes of Packet B
    };

    // Burst 2: Remaining 9 bytes of Packet B + Complete Packet C
    uint8_t burst2[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDE,       // Second 9 bytes of Packet B (includes checksum)
        0xFF, 0x55, 0x02, 0x00, 0x02, 0xFC                         // Complete Packet C
    };

    // Step 1: Process Burst 1
    size_t processed1 = esp.processRawBuffer(burst1, sizeof(burst1));
    TEST_ASSERT(processed1 == sizeof(burst1), "All bytes of Burst 1 should be processed");
    TEST_ASSERT(esp.isRxIncomplete(), "Accumulator MUST be in mid-packet state for Packet B");
    TEST_ASSERT(esp.getAccumulatorCursor() == 9, "Accumulator cursor must be exactly 9 bytes into Packet B");
    TEST_ASSERT(esp.getAccumulatorExpectedLen() == 0x0E, "Expected length for Packet B must be 14 (0x0E)");

    // Step 2: Process Burst 2
    size_t processed2 = esp.processRawBuffer(burst2, sizeof(burst2));
    TEST_ASSERT(processed2 == sizeof(burst2), "All bytes of Burst 2 should be processed");
    TEST_ASSERT(!esp.isRxIncomplete(), "Accumulator should be idle after Burst 2");

    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 3, "All 3 packets (A, B, C) must be successfully queued");

    // Verify Packet A
    TEST_ASSERT(pkts[0].size() == 6, "Packet A size must be 6 bytes");
    TEST_ASSERT(pkts[0][4] == 0x01, "Packet A payload check");

    // Verify Packet B
    TEST_ASSERT(pkts[1].size() == 18, "Packet B size must be 18 bytes");
    TEST_ASSERT(pkts[1][0] == 0xFF && pkts[1][1] == 0x55, "Packet B header");
    TEST_ASSERT(pkts[1][2] == 0x0E, "Packet B len");
    TEST_ASSERT(pkts[1][4] == 0x13, "Packet B command ID");
    TEST_ASSERT(pkts[1][17] == 0xDE, "Packet B checksum");

    // Verify Packet C
    TEST_ASSERT(pkts[2].size() == 6, "Packet C size must be 6 bytes");
    TEST_ASSERT(pkts[2][4] == 0x02, "Packet C payload check");
    TEST_ASSERT(pkts[2][5] == 0xFC, "Packet C checksum");
}

// Test 7: Preamble split across burst boundary (Burst 1 ends with 0xFF, Burst 2 starts with 0x55)
void test_preamble_split_across_bursts() {
    esPod esp(1, -1, -1, 19200);

    // Burst 1 ends with 0xFF
    uint8_t burst1[] = { 0x10, 0x20, 0xFF };
    esp.processRawBuffer(burst1, sizeof(burst1));
    TEST_ASSERT(!esp.isRxIncomplete(), "Single 0xFF should not trigger rxIncomplete yet");

    // Burst 2 starts with 0x55 + length + payload + checksum
    uint8_t burst2[] = { 0x55, 0x02, 0x00, 0x01, 0xFD };
    esp.processRawBuffer(burst2, sizeof(burst2));
    TEST_ASSERT(!esp.isRxIncomplete(), "Packet should complete cleanly");
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 1, "Packet assembled across split preamble");
    TEST_ASSERT(pkts[0].size() == 6, "Packet length");
}

// Test 8: Stress-test random chunk slicing across multiple packets
void test_random_chunk_slicing_stress() {
    esPod esp(1, -1, -1, 19200);

    // Construct a continuous stream of 5 valid packets
    std::vector<uint8_t> stream;
    for (uint8_t i = 1; i <= 5; i++) {
        stream.push_back(0xFF);
        stream.push_back(0x55);
        stream.push_back(0x02);
        stream.push_back(0x00);
        stream.push_back(i);
        uint32_t sum = 0x02 + 0x00 + i;
        stream.push_back((uint8_t)(0x100 - (sum & 0xFF)));
    }

    // Slice stream into varying chunk sizes: [1, 3, 2, 7, 4, 1, ...]
    size_t chunkSizes[] = { 1, 3, 2, 7, 4, 1, 5, 2, 3, 2 };
    size_t chunkIndex = 0;
    size_t streamOffset = 0;

    while (streamOffset < stream.size()) {
        size_t thisChunk = chunkSizes[chunkIndex % 10];
        if (streamOffset + thisChunk > stream.size()) {
            thisChunk = stream.size() - streamOffset;
        }
        esp.processRawBuffer(&stream[streamOffset], thisChunk);
        streamOffset += thisChunk;
        chunkIndex++;
    }

    TEST_ASSERT(!esp.isRxIncomplete(), "All packets completed");
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 5, "All 5 packets reconstructed accurately");
    for (uint8_t i = 1; i <= 5; i++) {
        TEST_ASSERT(pkts[i - 1][4] == i, "Payload verification");
    }
}

// Test 9: Inter-packet garbage resilience
void test_garbage_bytes_between_packets() {
    esPod esp(1, -1, -1, 19200);
    uint8_t noisyStream[] = {
        0xAA, 0xBB, 0xCC, 0xFF, 0x01, 0x02, // Garbage with dangling 0xFF
        0xFF, 0x55, 0x02, 0x00, 0x01, 0xFD, // Pkt 1
        0xEE, 0xDD, 0x00,                   // Inter-packet garbage
        0xFF, 0x55, 0x02, 0x00, 0x02, 0xFC, // Pkt 2
        0x00, 0x11                          // Trailing garbage
    };

    esp.processRawBuffer(noisyStream, sizeof(noisyStream));
    auto pkts = drainRingBuffer(esp);
    TEST_ASSERT(pkts.size() == 2, "Both packets parsed despite noise");
    TEST_ASSERT(pkts[0][4] == 0x01, "Pkt 1 payload");
    TEST_ASSERT(pkts[1][4] == 0x02, "Pkt 2 payload");
}

int main() {
    printf("========================================================\n");
    printf("  REAL esPod Production Component Regression Test Suite \n");
    printf("========================================================\n");

    RUN_TEST(test_mini_cooper_1byte_stream);
    RUN_TEST(test_multi_packet_contiguous);
    RUN_TEST(test_bad_checksum_rejection);
    RUN_TEST(test_timeout_reset_recovery);
    RUN_TEST(test_invalid_length_byte);
    RUN_TEST(test_mixed_consolidated_data_streams_burst);
    RUN_TEST(test_preamble_split_across_bursts);
    RUN_TEST(test_random_chunk_slicing_stress);
    RUN_TEST(test_garbage_bytes_between_packets);

    printf("========================================================\n");
    printf("  Results: %d/%d tests passed successfully!             \n", passedTests, totalTests);
    printf("========================================================\n");
    return 0;
}
