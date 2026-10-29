#!/usr/bin/env bash
# Runs the verifier on every <folder>/<name>Input<Good|Bad>.txt + <name>Output<Good|Bad>.txt pair.
# Good pairs must be accepted (exit 0), Bad pairs must be rejected (non-zero exit).
# Usage: ./run_tests.sh [-v] [folder]
#   -v      also print the verifier's message for passing tests
#   folder  test folder; if omitted, you are asked for it (Enter = TestSamples)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_DIR="TestSamples"

VERBOSE=0
TEST_DIR=""
for arg in "$@"; do
    case "$arg" in
        -v) VERBOSE=1 ;;
        -h|--help) sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)  TEST_DIR="$arg" ;;
    esac
done

if [ -z "$TEST_DIR" ]; then
    if [ -t 0 ]; then
        read -r -p "Test folder [$DEFAULT_DIR]: " TEST_DIR
    fi
    TEST_DIR="${TEST_DIR:-$DEFAULT_DIR}"
fi
TEST_DIR="${TEST_DIR%/}"

# Accept the folder relative to where you run from, or relative to this script
if [ ! -d "$TEST_DIR" ] && [ -d "$SCRIPT_DIR/$TEST_DIR" ]; then
    TEST_DIR="$SCRIPT_DIR/$TEST_DIR"
fi

# Colors (disabled when output is not a terminal)
if [ -t 1 ]; then
    GREEN=$'\e[32m'; RED=$'\e[31m'; YELLOW=$'\e[33m'; CYAN=$'\e[36m'
    DIM=$'\e[2m'; BOLD=$'\e[1m'; RESET=$'\e[0m'
else
    GREEN=; RED=; YELLOW=; CYAN=; DIM=; BOLD=; RESET=
fi

if [ ! -d "$TEST_DIR" ]; then
    echo "${RED}${BOLD}Folder not found:${RESET} $TEST_DIR"
    exit 1
fi

BIN="$(mktemp -d)/verifier"
trap 'rm -rf "$(dirname "$BIN")"' EXIT

echo "${BOLD}${CYAN}==> Compiling verifier.cpp${RESET}"
if ! g++ -std=c++17 -O2 -Wall -Wextra -o "$BIN" "$SCRIPT_DIR/verifier.cpp"; then
    echo "${RED}${BOLD}Compilation failed${RESET}"
    exit 1
fi
echo "${BOLD}${CYAN}==> Running tests from ${TEST_DIR}/${RESET}"
echo

shopt -s nullglob
inputs=("$TEST_DIR"/*InputGood.txt "$TEST_DIR"/*InputBad.txt)
if [ ${#inputs[@]} -eq 0 ]; then
    echo "${YELLOW}No tests found in ${TEST_DIR}/${RESET}"
    exit 1
fi

passed=0
failed=0
failed_names=()

printf "${BOLD}%-6s  %-6s  %-34s  %s${RESET}\n" "RESULT" "EXPECT" "TEST" "VERIFIER SAYS"
printf "${DIM}%s${RESET}\n" "--------------------------------------------------------------------------------"

for in_file in "${inputs[@]}"; do
    base="$(basename "$in_file" .txt)"
    if [[ $base == *Good ]]; then kind="Good"; expect="accept"; else kind="Bad"; expect="reject"; fi
    # Only the "Input<Good|Bad>" suffix is the marker, the test name itself may contain "Input"
    name="${base%Input$kind}"
    out_file="$TEST_DIR/${name}Output${kind}.txt"

    if [ ! -f "$out_file" ]; then
        printf "${YELLOW}%-6s${RESET}  %-6s  %-34s  %s\n" "MISS" "$expect" "$name" "no matching $(basename "$out_file")"
        failed=$((failed + 1)); failed_names+=("$name")
        continue
    fi

    msg="$("$BIN" "$in_file" "$out_file" 2>&1)"
    code=$?
    msg="${msg%%$'\n'*}"
    if [ "$code" -eq 0 ]; then got="accept"; else got="reject"; fi

    if [ "$got" = "$expect" ]; then
        passed=$((passed + 1))
        status="${GREEN}PASS${RESET}  "
        detail="${DIM}${msg}${RESET}"
        [ $VERBOSE -eq 0 ] && detail=""
    else
        failed=$((failed + 1)); failed_names+=("$name")
        status="${RED}FAIL${RESET}  "
        detail="${RED}got ${got}: ${msg}${RESET}"
    fi
    printf "%s  %-6s  %-34s  %s\n" "$status" "$expect" "$name" "$detail"
done

total=$((passed + failed))
echo
printf "${DIM}%s${RESET}\n" "--------------------------------------------------------------------------------"
if [ $failed -eq 0 ]; then
    echo "${GREEN}${BOLD}All ${total} tests passed${RESET}"
else
    echo "${RED}${BOLD}${failed}/${total} tests failed:${RESET} ${failed_names[*]}"
    echo "${GREEN}${passed}/${total} passed${RESET}"
    exit 1
fi
