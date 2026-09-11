# superPod Regression Test Suite

Automated, cross-platform regression test suite for `superPod` and `esPod` packet handling.
Tests verify protocol stream accumulation, multi-packet burst ingestion, arbitrary chunk fragmentation, checksum validation, timeout resets, and error resilience.

## Supported Platforms
- **Ubuntu / Debian / Fedora / Arch Linux** (`gcc` / `clang`)
- **macOS** (Apple Silicon `arm64` & Intel `x86_64`, Apple Clang)

## Quick Start

Execute the automated test runner:

```bash
./tests/run_tests.sh
```

Or run directly within the component:

```bash
cd components/espod/test
./run_tests.sh
```

## Git Hooks Integration

The repository includes pre-configured Git hooks to run these regression tests automatically:
- **`pre-commit`**: Automatically runs `./tests/run_tests.sh` before any commit. If tests fail, the commit is aborted.
- **`pre-push`**: Automatically runs `./tests/run_tests.sh` before pushing to a remote.

To activate the hooks on a new clone:

```bash
./.githooks/install.sh
```

## Test Coverage

1. **`test_mini_cooper_1byte_stream`**: Verifies exact Mini Cooper 19-byte iAP Identify frame (`FF FF 55 0E ... DE`) fed byte-by-byte.
2. **`test_multi_packet_contiguous`**: Verifies back-to-back packets within a single buffer.
3. **`test_bad_checksum_rejection`**: Verifies corrupted checksums are discarded without queueing.
4. **`test_timeout_reset_recovery`**: Verifies 500 ms inter-byte timeout resets cleanly and admits subsequent frames.
5. **`test_invalid_length_byte`**: Verifies length 0 or out-of-bounds frames are discarded.
6. **`test_mixed_consolidated_data_streams_burst`**: Stress-tests burst ingestion where Burst 1 contains 1.5 packets (one full packet + half of a second packet), and Burst 2 contains the second half + another full packet.
7. **`test_preamble_split_across_bursts`**: Verifies preamble bytes (`0xFF` in Burst 1, `0x55` in Burst 2) properly sync across burst boundaries.
8. **`test_random_chunk_slicing_stress`**: Slices continuous packet streams into random chunk sizes (1–7 bytes).
9. **`test_garbage_bytes_between_packets`**: Verifies noise before, between, and after packets is discarded without dropping valid packets.
