#!/usr/bin/env bash
# ==============================================================================
# Self-Contained Regression Test Runner for esPod Component
# Directly compiles and tests production source files in ../src/
# Compatible with: Linux (Ubuntu, Debian, Fedora, Arch) and macOS (Darwin)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESPOD_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"
TARGET_EXE="${BIN_DIR}/test_stream_accumulator"

# Production component sources
SRC_FILES=(
    "${SCRIPT_DIR}/test_stream_accumulator.cpp"
    "${ESPOD_DIR}/src/esPod.cpp"
    "${ESPOD_DIR}/src/L0x00.cpp"
    "${ESPOD_DIR}/src/L0x03.cpp"
    "${ESPOD_DIR}/src/L0x04.cpp"
)

# Include paths
INCLUDES=(
    "-I${ESPOD_DIR}/include"
    "-I${SCRIPT_DIR}/mock_idf"
)

# Color support detection
if [ -t 1 ]; then
    GREEN="\033[0;32m"
    RED="\033[0;31m"
    CYAN="\033[0;36m"
    BOLD="\033[1m"
    RESET="\033[0m"
else
    GREEN=""
    RED=""
    CYAN=""
    BOLD=""
    RESET=""
fi

echo -e "${BOLD}${CYAN}>>> esPod Component Regression Test Suite <<<${RESET}"
echo "Host OS: $(uname -s) ($(uname -m))"

# Compiler discovery (supports clang++, g++, or standard c++)
if [ -z "${CXX:-}" ]; then
    if command -v clang++ >/dev/null 2>&1; then
        CXX="clang++"
    elif command -v g++ >/dev/null 2>&1; then
        CXX="g++"
    elif command -v c++ >/dev/null 2>&1; then
        CXX="c++"
    else
        echo -e "${RED}Error: No suitable C++ compiler (clang++, g++, c++) found in PATH.${RESET}" >&2
        exit 1
    fi
fi

echo "Using C++ Compiler: ${CXX} ($(${CXX} --version | head -n 1))"

mkdir -p "${BIN_DIR}"

echo -e "\nCompiling tests against ${ESPOD_DIR}/src/ ..."
${CXX} -std=c++14 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
    "${INCLUDES[@]}" \
    "${SRC_FILES[@]}" \
    -o "${TARGET_EXE}"

echo -e "${GREEN}Compilation successful.${RESET}\n"

# Run tests
"${TARGET_EXE}"
TEST_EXIT_CODE=$?

if [ ${TEST_EXIT_CODE} -eq 0 ]; then
    echo -e "\n${BOLD}${GREEN}✔ esPod regression test suite PASSED (100% assertions verified).${RESET}\n"
else
    echo -e "\n${BOLD}${RED}✘ esPod regression test suite FAILED with exit code ${TEST_EXIT_CODE}.${RESET}\n"
fi

exit ${TEST_EXIT_CODE}
