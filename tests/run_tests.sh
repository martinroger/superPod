#!/usr/bin/env bash
# ==============================================================================
# superPod Umbrella Regression Test Runner
# Discovers and executes all component-level and system-level test suites.
# Compatible with: Linux (Ubuntu, Debian, Fedora, Arch) and macOS (Darwin)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

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

echo -e "${BOLD}${CYAN}================================================================${RESET}"
echo -e "${BOLD}${CYAN}            superPod Firmware Regression Test Suite             ${RESET}"
echo -e "${BOLD}${CYAN}================================================================${RESET}"
echo "Host OS: $(uname -s) ($(uname -m))"

FAILURES=0

# 1. Component Tests: esPod
echo -e "\n${BOLD}>>> Running Component Test Suite: components/espod ...${RESET}"
if [ -f "${REPO_ROOT}/components/espod/test/run_tests.sh" ]; then
    if "${REPO_ROOT}/components/espod/test/run_tests.sh"; then
        echo -e "${GREEN}✔ components/espod tests passed.${RESET}"
    else
        echo -e "${RED}✘ components/espod tests failed.${RESET}"
        FAILURES=$((FAILURES + 1))
    fi
else
    echo -e "${RED}Error: components/espod/test/run_tests.sh not found.${RESET}"
    FAILURES=$((FAILURES + 1))
fi

# Summary
echo -e "\n${BOLD}${CYAN}================================================================${RESET}"
if [ ${FAILURES} -eq 0 ]; then
    echo -e "${BOLD}${GREEN}✔ ALL TEST SUITES PASSED SUCCESSFULLY!${RESET}"
    echo -e "${BOLD}${CYAN}================================================================${RESET}\n"
    exit 0
else
    echo -e "${BOLD}${RED}✘ ${FAILURES} TEST SUITE(S) FAILED.${RESET}"
    echo -e "${BOLD}${CYAN}================================================================${RESET}\n"
    exit 1
fi
