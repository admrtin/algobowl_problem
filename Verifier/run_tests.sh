#!/usr/bin/env bash
# Verify multiple output solutions against ONE input_group<N>.txt board.
# Usage: ./run_tests.sh [-v] [--all] [group] [outputs_folder]
#   -v      show verifier messages for passing outputs
#   --all   verify every *.txt in the output folder (even other group names)
#   group   target group number; prompted if omitted
#   folder  output folder, default: ../TeamsOutputs relative to this script

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DEFAULT_DIR="$PROJECT_DIR/TeamsOutputs"

VERBOSE=0
CHECK_ALL=0
GROUP=""
OUTPUT_DIR=""

for arg in "$@"; do
    case "$arg" in
        -v) VERBOSE=1 ;;
        --all) CHECK_ALL=1 ;;
        -h|--help) sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) echo "Unknown option: $arg" >&2; exit 1 ;;
        *)
            if [ -z "$GROUP" ]; then GROUP="$arg"
            elif [ -z "$OUTPUT_DIR" ]; then OUTPUT_DIR="$arg"
            else echo "Too many arguments" >&2; exit 1
            fi
            ;;
    esac
done

if [ -z "$GROUP" ]; then
    read -r -p "Input group number (e.g. 1294): " GROUP
fi
if [[ ! "$GROUP" =~ ^[0-9]+$ ]]; then
    echo "Invalid group number: $GROUP" >&2
    exit 1
fi

INPUT_FILE="$PROJECT_DIR/TeamsInputs/input_group${GROUP}.txt"
if [ ! -f "$INPUT_FILE" ]; then
    echo "Input file not found: $INPUT_FILE" >&2
    exit 1
fi

if [ -z "$OUTPUT_DIR" ]; then
    OUTPUT_DIR="$DEFAULT_DIR"
elif [ ! -d "$OUTPUT_DIR" ] && [ -d "$SCRIPT_DIR/$OUTPUT_DIR" ]; then
    OUTPUT_DIR="$SCRIPT_DIR/$OUTPUT_DIR"
fi
OUTPUT_DIR="${OUTPUT_DIR%/}"
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "Output folder not found: $OUTPUT_DIR" >&2
    exit 1
fi

if [ -t 1 ]; then
    GREEN=$'\e[32m'; RED=$'\e[31m'; YELLOW=$'\e[33m'; CYAN=$'\e[36m'
    DIM=$'\e[2m'; BOLD=$'\e[1m'; RESET=$'\e[0m'
else
    GREEN=; RED=; YELLOW=; CYAN=; DIM=; BOLD=; RESET=
fi

BIN_DIR="$(mktemp -d)"
trap 'rm -rf "$BIN_DIR"' EXIT
BIN="$BIN_DIR/verifier"

echo "${BOLD}${CYAN}==> Compiling verifier.cpp${RESET}"
if ! g++ -std=c++17 -O2 -Wall -Wextra -o "$BIN" "$SCRIPT_DIR/verifier.cpp"; then
    echo "${RED}${BOLD}Compilation failed${RESET}"
    exit 1
fi

echo "${BOLD}${CYAN}==> Input: $INPUT_FILE${RESET}"
echo "${BOLD}${CYAN}==> Outputs: $OUTPUT_DIR${RESET}"
echo

shopt -s nullglob
if [ "$CHECK_ALL" -eq 1 ]; then
    outputs=("$OUTPUT_DIR"/*.txt)
else
    # Match only outputs intended for the requested input group.
    outputs=(
        "$OUTPUT_DIR"/output_from_*_to_"$GROUP".txt
        "$OUTPUT_DIR"/output_group"$GROUP".txt
        "$OUTPUT_DIR"/output_"$GROUP".txt
    )
fi

# Patterns without '*' stay literal even with nullglob. Keep only real files.
existing_outputs=()
for file in "${outputs[@]}"; do
    [ -f "$file" ] && existing_outputs+=("$file")
done
outputs=("${existing_outputs[@]}")

if [ ${#outputs[@]} -eq 0 ]; then
    echo "${YELLOW}No matching outputs for group $GROUP in $OUTPUT_DIR${RESET}"
    echo "Use --all to test every .txt file in the folder."
    exit 1
fi

passed=0
failed=0
best_score=-1
best_file=""

printf "${BOLD}%-7s  %-12s  %-44s  %s${RESET}\n" "RESULT" "VIOLATIONS" "OUTPUT" "VERIFIER SAYS"
printf "${DIM}%s${RESET}\n" "------------------------------------------------------------------------------------------------------"

for out_file in "${outputs[@]}"; do
    filename="$(basename "$out_file")"
    msg="$("$BIN" "$INPUT_FILE" "$out_file" 2>&1)"
    code=$?
    msg="${msg%%$'\n'*}"

    if [ "$code" -eq 0 ]; then
        read -r reported _ < "$out_file"
        passed=$((passed + 1))
        if [[ "$reported" =~ ^[0-9]+$ ]] && { [ "$best_score" -lt 0 ] || (( 10#$reported < best_score )); }; then
            best_score=$((10#$reported))
            best_file="$filename"
        fi
        detail=""
        [ "$VERBOSE" -eq 1 ] && detail="${DIM}${msg}${RESET}"
        printf "${GREEN}%-7s${RESET}  %-12s  %-44s  %s\n" "PASS" "$reported" "$filename" "$detail"
    else
        failed=$((failed + 1))
        printf "${RED}%-7s${RESET}  %-12s  %-44s  ${RED}%s${RESET}\n" "FAIL" "-" "$filename" "$msg"
    fi
done

total=$((passed + failed))
echo
printf "${DIM}%s${RESET}\n" "------------------------------------------------------------------------------------------------------"
echo "${GREEN}${passed}/${total} outputs passed${RESET}; ${RED}${failed}/${total} failed${RESET}"
if [ -n "$best_file" ]; then
    echo "${BOLD}BEST VALID SOLUTION: $best_file ($best_score violations)${RESET}"
fi

[ "$failed" -eq 0 ]
