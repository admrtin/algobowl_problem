#!/usr/bin/env bash
# run the genetic algorithm on every input in TeamsInputs/ in parallel.
# every interval_seconds, a table of the currently running inputs is printed
# (latest generation, violations, unlit, elapsed, and the change in violations
# since the previous table), and a summary table is printed once every run finishes.
#
# usage: ./run_all.sh [time_limit_seconds=60] [seed=1] [population_size=50] [max_jobs=nproc] [interval_seconds=10]
# outputs go to out/genetic/output_<input name>.txt, full logs to out/genetic/logs/

set -uo pipefail

TIME_LIMIT=${1:-60}
SEED=${2:-1}
POPULATION=${3:-50}
MAX_JOBS=${4:-$(nproc)} # more jobs than cores would give each run less cpu inside the same time limit
INTERVAL=${5:-10}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$SCRIPT_DIR")
INPUT_DIR="$ROOT/TeamsInputs"
OUT_DIR="$ROOT/out/genetic"
LOG_DIR="$OUT_DIR/logs"
BINARY="$SCRIPT_DIR/main"

mkdir -p "$LOG_DIR"

echo "building $BINARY"
g++ -O2 -o "$BINARY" "$SCRIPT_DIR/main.cpp" "$SCRIPT_DIR/genetic.cpp" "$SCRIPT_DIR/akari_io.cpp" || exit 1

inputs=("$INPUT_DIR"/*.txt)
echo "running ${#inputs[@]} inputs, ${MAX_JOBS} at a time (time_limit=${TIME_LIMIT}s seed=${SEED} population=${POPULATION})"

# one run, all output into its log file (line-buffered so progress shows up in the log as it happens)
run_one() {
    local input=$1
    local name
    name=$(basename "$input" .txt)
    name=${name#input_}
    stdbuf -oL "$BINARY" "$input" "$OUT_DIR/output_${name}.txt" "$SEED" "$TIME_LIMIT" "$POPULATION" > "$LOG_DIR/${name}.log" 2>&1
}

# clear logs from any previous run so stale runs don't show up as active
for input in "${inputs[@]}"; do
    name=$(basename "$input" .txt)
    rm -f "$LOG_DIR/${name#input_}.log"
done

STAT_FMT="%-14s %10s %10s %8s %10s %8s\n"
declare -A last_violations

# table of every input that has started but not finished, from the latest progress line in its log
print_stats() {
    local input name log line change done_count=0 active=()
    for input in "${inputs[@]}"; do
        name=$(basename "$input" .txt)
        name=${name#input_}
        log="$LOG_DIR/${name}.log"
        [[ -e $log ]] || continue
        if grep -q '^stopped after' "$log"; then done_count=$((done_count + 1)); continue; fi
        active+=("$name")
    done

    echo
    echo "[$(printf '%(%H:%M:%S)T' -1)] ${#active[@]} running, ${done_count}/${#inputs[@]} finished"
    (( ${#active[@]} )) || return
    printf "$STAT_FMT" "input" "generation" "violations" "unlit" "elapsed" "change"
    for name in "${active[@]}"; do
        line=$(grep '^generation ' "$LOG_DIR/${name}.log" | tail -n 1)
        if [[ $line =~ ^generation\ ([0-9]+).*violations:\ ([0-9]+).*unlit:\ ([0-9]+).*elapsed:\ ([0-9.]+)s ]]; then
            local v=${BASH_REMATCH[2]}
            change="-"
            [[ -n ${last_violations[$name]:-} ]] && change=$(( v - last_violations[$name] ))
            last_violations[$name]=$v
            printf "$STAT_FMT" "$name" "${BASH_REMATCH[1]}" "$v" "${BASH_REMATCH[3]}" "${BASH_REMATCH[4]}s" "$change"
        else
            printf "$STAT_FMT" "$name" "-" "-" "-" "-" "-"
        fi
    done
}

start=$SECONDS
# launch runs in the background, at most MAX_JOBS at a time
(
    for input in "${inputs[@]}"; do
        while (( $(jobs -rp | wc -l) >= MAX_JOBS )); do wait -n; done
        run_one "$input" &
    done
    wait
) &
launcher=$!

# print stats every INTERVAL seconds until every run is done (checking once a second so we exit promptly)
next=$((SECONDS + INTERVAL))
while kill -0 "$launcher" 2>/dev/null; do
    if (( SECONDS >= next )); then
        print_stats
        next=$((SECONDS + INTERVAL))
    fi
    sleep 1
done
wait "$launcher"

# summary, one row per input, from the "final violations" line of each log
echo
echo "finished all runs in $((SECONDS - start))s"
printf "%-14s %10s %8s %12s\n" "input" "violations" "unlit" "generations"
for input in "${inputs[@]}"; do
    name=$(basename "$input" .txt)
    name=${name#input_}
    log="$LOG_DIR/${name}.log"
    violations=$(grep -oP 'final violations: \K[0-9]+' "$log" 2>/dev/null || echo "FAILED")
    unlit=$(grep -oP 'unlit: \K[0-9]+' <(grep 'final violations' "$log" 2>/dev/null) || echo "-")
    generations=$(grep -oP 'stopped after \K[0-9]+' "$log" 2>/dev/null || echo "-")
    printf "%-14s %10s %8s %12s\n" "$name" "$violations" "$unlit" "$generations"
done
