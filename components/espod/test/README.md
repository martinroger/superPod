# esPod Component Regression Test Suite

Self-contained, cross-platform regression test suite for `esPod`.
Directly exercises the production component C++ sources (`esPod.cpp`, `L0x00.cpp`, `L0x03.cpp`, `L0x04.cpp`) under host `clang++` or `g++` without needing hardware or the full ESP-IDF toolchain.

## Supported Platforms
- **Ubuntu / Debian / Fedora / Arch Linux** (`gcc` / `clang`)
- **macOS** (Apple Silicon `arm64` & Intel `x86_64`, Apple Clang)

## Running Tests
Run the standalone script from this directory or repository root:

```bash
./run_tests.sh
```

## Test Coverage
1. `test_mini_cooper_1byte_stream`: Exact 19-byte Mini Cooper trace packet assembled from 1-byte chunks.
2. `test_multi_packet_contiguous`: Multiple back-to-back packets within a single buffer.
3. `test_bad_checksum_rejection`: Corrupted checksum packets safely discarded.
4. `test_timeout_reset_recovery`: Incomplete frame timeout reset recovery.
5. `test_invalid_length_byte`: Rejection of length 0 frames.
6. `test_mixed_consolidated_data_streams_burst`: 1.5 packet burst followed by second half + subsequent packet.
7. `test_preamble_split_across_bursts`: `0xFF` in Burst 1 and `0x55` in Burst 2 sync across burst boundary.
8. `test_random_chunk_slicing_stress`: Arbitrary chunk slicing (1–7 bytes).
9. `test_garbage_bytes_between_packets`: Inter-packet line noise resilience.
