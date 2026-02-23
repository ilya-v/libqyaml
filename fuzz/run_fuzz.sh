#!/bin/bash
# Run fuzz harnesses with all output directed to test-output/fuzz/
# Usage: ./fuzz/run_fuzz.sh [harness] [max_total_time]
#   harness: scan, parse, load, structured (default: all)
#   max_total_time: seconds per harness (default: 300)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_DIR}/test-output/fuzz"
BIN_DIR="${OUTPUT_DIR}/bin"

HARNESS="${1:-all}"
MAX_TIME="${2:-300}"

# Ensure output directories exist
mkdir -p "${OUTPUT_DIR}/corpus_scan"
mkdir -p "${OUTPUT_DIR}/corpus_parse"
mkdir -p "${OUTPUT_DIR}/corpus_load"
mkdir -p "${OUTPUT_DIR}/corpus_structured"
mkdir -p "${OUTPUT_DIR}/crashes"
mkdir -p "${OUTPUT_DIR}/logs"
mkdir -p "${BIN_DIR}"

# Build if needed
if [ ! -f "${BIN_DIR}/fuzz_scan" ] || [ ! -f "${BIN_DIR}/fuzz_parse" ] || \
   [ ! -f "${BIN_DIR}/fuzz_load" ] || [ ! -f "${BIN_DIR}/fuzz_parse_structured" ]; then
    echo "Building fuzz harnesses..."
    "${SCRIPT_DIR}/build.sh"
fi

run_harness() {
    local name="$1"
    local binary="${BIN_DIR}/fuzz_${name}"
    local corpus_dir="${OUTPUT_DIR}/corpus_${name}"
    local log_file="${OUTPUT_DIR}/logs/fuzz-${name}.log"
    local seed_dir="${SCRIPT_DIR}/seeds"

    if [ ! -f "$binary" ]; then
        echo "ERROR: Binary $binary not found. Run fuzz/build.sh first."
        return 1
    fi

    echo "Running fuzz_${name} for ${MAX_TIME}s..."
    echo "  Corpus: ${corpus_dir}"
    echo "  Log: ${log_file}"
    echo "  Crashes: ${OUTPUT_DIR}/crashes/"

    # Run from test-output/fuzz so any stray files land there
    cd "${OUTPUT_DIR}"

    "$binary" \
        "$corpus_dir" \
        "$seed_dir" \
        -max_total_time="${MAX_TIME}" \
        -artifact_prefix="${OUTPUT_DIR}/crashes/" \
        -print_final_stats=1 \
        2>"$log_file" || true

    echo "  Done. See ${log_file} for details."
}

if [ "$HARNESS" = "all" ]; then
    for h in scan parse load structured; do
        run_harness "$h"
    done
else
    run_harness "$HARNESS"
fi

echo ""
echo "All fuzz runs complete. Output in: ${OUTPUT_DIR}/"
echo "Logs: ${OUTPUT_DIR}/logs/"
echo "Crashes: ${OUTPUT_DIR}/crashes/"
