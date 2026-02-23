#!/usr/bin/env bash
#
# Per-commit fuzz script for the validation pipeline.
# Maintained by the strategic-tester. The tester runs this as-is.
#
# Usage: ./fuzz/run-per-commit-fuzz.sh
#   No arguments required. Uses up to 3 cores.
#   Reads harness list from fuzz/fuzz-harnesses.list.
#   Outputs structured results to stdout.
#   Exit code: 0 = all clean, 1 = crashes/divergences found, 2 = build error
#
# Time budget: ~30 seconds total (part of the 2-minute validation budget).

set -uo pipefail
# Note: -e is intentionally NOT set. Fuzzers exit non-zero on crashes,
# which we handle per-harness rather than aborting the whole script.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HARNESS_LIST="$SCRIPT_DIR/fuzz-harnesses.list"
BUILD_DIR="$PROJECT_DIR/test-output/fuzz-build"
CORPUS_DIR="$PROJECT_DIR/test-output/fuzz-corpus-percommit"
CRASH_DIR="$PROJECT_DIR/test-output/fuzz-crashes-percommit"
SEED_DIR="$PROJECT_DIR/test-output/fuzz-seeds-combined"

# Collect all seed sources into a single directory for the fuzzer.
# Sources: fuzz/seeds/, yaml-test-data, yaml-test-suite, yaml-fuzz
collect_seeds() {
    mkdir -p "$SEED_DIR"
    # Only rebuild seeds if directory is empty or older than 1 hour
    local seed_count
    seed_count=$(find "$SEED_DIR" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
    if [ "$seed_count" -gt 100 ]; then
        return 0
    fi

    # Fuzz harness seeds
    if [ -d "$SCRIPT_DIR/seeds" ]; then
        cp -n "$SCRIPT_DIR/seeds"/* "$SEED_DIR/" 2>/dev/null || true
    fi

    # yaml-test-data: in.yaml files
    local ref_dir="$PROJECT_DIR/reference"
    if [ -d "$ref_dir/yaml-test-data" ]; then
        find "$ref_dir/yaml-test-data" -name 'in.yaml' -type f -exec cp -n {} "$SEED_DIR/" \; 2>/dev/null || true
        # Rename to avoid collisions (each dir has in.yaml)
        local i=0
        find "$ref_dir/yaml-test-data" -name 'in.yaml' -type f | while read -r f; do
            local dir_name
            dir_name=$(basename "$(dirname "$f")")
            cp -n "$f" "$SEED_DIR/ytd-${dir_name}.yaml" 2>/dev/null || true
        done
    fi

    # yaml-test-suite: src/*/in-yaml files
    if [ -d "$ref_dir/yaml-test-suite/src" ]; then
        find "$ref_dir/yaml-test-suite/src" -name 'in-yaml' -type f | while read -r f; do
            local dir_name
            dir_name=$(basename "$(dirname "$f")")
            cp -n "$f" "$SEED_DIR/yts-${dir_name}.yaml" 2>/dev/null || true
        done
    fi

    # yaml-fuzz: minimized and raw testcases
    if [ -d "$ref_dir/yaml-fuzz/min_testcases" ]; then
        for f in "$ref_dir/yaml-fuzz/min_testcases"/*; do
            [ -f "$f" ] && cp -n "$f" "$SEED_DIR/yfm-$(basename "$f")" 2>/dev/null || true
        done
    fi
    if [ -d "$ref_dir/yaml-fuzz/raw_testcases" ]; then
        for f in "$ref_dir/yaml-fuzz/raw_testcases"/*; do
            [ -f "$f" ] && cp -n "$f" "$SEED_DIR/yfr-$(basename "$f")" 2>/dev/null || true
        done
    fi

    seed_count=$(find "$SEED_DIR" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
    echo "Seeds collected: $seed_count files"
}

# Time per harness (seconds). Divided equally among harnesses.
TOTAL_FUZZ_TIME=24
# Max cores for parallel fuzzing
MAX_CORES=3

# Parse harness list
declare -a HARNESS_NAMES=()
declare -a HARNESS_SOURCES=()
declare -a HARNESS_EXTRA_FLAGS=()

while IFS= read -r line; do
    # Skip comments and empty lines
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line// /}" ]] && continue

    name=$(echo "$line" | awk '{print $1}')
    source=$(echo "$line" | awk '{print $2}')
    extra=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i}' | sed 's/ *$//')

    HARNESS_NAMES+=("$name")
    HARNESS_SOURCES+=("$source")
    HARNESS_EXTRA_FLAGS+=("$extra")
done < "$HARNESS_LIST"

NUM_HARNESSES=${#HARNESS_NAMES[@]}
if [ "$NUM_HARNESSES" -eq 0 ]; then
    echo "ERROR: No harnesses found in $HARNESS_LIST"
    exit 2
fi

TIME_PER_HARNESS=$((TOTAL_FUZZ_TIME / NUM_HARNESSES))
if [ "$TIME_PER_HARNESS" -lt 5 ]; then
    TIME_PER_HARNESS=5
fi

echo "=== Per-Commit Fuzz ==="
echo "Harnesses: $NUM_HARNESSES"
echo "Time per harness: ${TIME_PER_HARNESS}s"
echo "Total budget: ~${TOTAL_FUZZ_TIME}s"
echo ""

# Step 0: Collect seed inputs from all external sources
collect_seeds
echo ""

# Step 1: Build the library with ASAN+fuzzer instrumentation
mkdir -p "$BUILD_DIR"
echo "Building library with ASAN..."
BUILD_START=$(date +%s)

# Check for clang
CLANG=$(command -v clang 2>/dev/null || true)
if [ -z "$CLANG" ]; then
    echo "ERROR: clang not found (required for fuzzing)"
    exit 2
fi

# Build library (suppress output)
BUILD_LOG="$BUILD_DIR/build.log"
(
    cd "$BUILD_DIR"
    cmake -DCMAKE_C_COMPILER=clang \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_FLAGS="-fsanitize=address,fuzzer-no-link -g" \
          -DQYAML_BUILD_TESTS=OFF \
          "$PROJECT_DIR" > "$BUILD_LOG" 2>&1
    make -j"$MAX_CORES" qyaml >> "$BUILD_LOG" 2>&1
)
if [ $? -ne 0 ]; then
    echo "ERROR: Library build failed. See $BUILD_LOG"
    exit 2
fi

# Build each harness
BUILT_COUNT=0
for i in "${!HARNESS_NAMES[@]}"; do
    name="${HARNESS_NAMES[$i]}"
    source="${HARNESS_SOURCES[$i]}"
    extra="${HARNESS_EXTRA_FLAGS[$i]}"

    "$CLANG" -fsanitize=fuzzer,address -O1 -g \
        -I"$PROJECT_DIR/include" \
        -DYAML_DECLARE_STATIC \
        "$SCRIPT_DIR/$source" \
        -L"$BUILD_DIR" -lqyaml \
        $extra \
        -o "$BUILD_DIR/$name" 2>/dev/null
    if [ $? -eq 0 ]; then
        BUILT_COUNT=$((BUILT_COUNT + 1))
    else
        echo "WARNING: Failed to build $name"
    fi
done

BUILD_END=$(date +%s)
BUILD_TIME=$((BUILD_END - BUILD_START))
echo "Build: ${BUILD_TIME}s ($BUILT_COUNT/$NUM_HARNESSES harnesses)"
echo ""

if [ "$BUILT_COUNT" -eq 0 ]; then
    echo "ERROR: No harnesses built successfully"
    exit 2
fi

# Step 2: Run each harness
TOTAL_EXECS=0
TOTAL_CRASHES=0
TOTAL_DIVERGENCES=0
TOTAL_TIME=0
OVERALL_RESULT=0

for i in "${!HARNESS_NAMES[@]}"; do
    name="${HARNESS_NAMES[$i]}"

    if [ ! -x "$BUILD_DIR/$name" ]; then
        echo "--- $name: SKIPPED (build failed) ---"
        continue
    fi

    # Create per-harness corpus and crash dirs
    h_corpus="$CORPUS_DIR/$name"
    h_crashes="$CRASH_DIR/$name"
    mkdir -p "$h_corpus" "$h_crashes"

    # Clean old crashes from previous runs
    rm -f "$h_crashes"/crash-* "$h_crashes"/oom-* "$h_crashes"/timeout-*

    echo "--- $name ---"
    FUZZ_START=$(date +%s)

    # Run the fuzzer. Both stdout and stderr go to FUZZ_OUTPUT.
    # The fuzzer may exit non-zero on crashes — that's expected.
    FUZZ_LOG="$BUILD_DIR/${name}.log"
    "$BUILD_DIR/$name" \
        -max_total_time="$TIME_PER_HARNESS" \
        -max_len=4096 \
        -artifact_prefix="$h_crashes/" \
        "$h_corpus" \
        "$SEED_DIR" \
        > "$FUZZ_LOG" 2>&1 || true

    FUZZ_END=$(date +%s)
    FUZZ_TIME=$((FUZZ_END - FUZZ_START))
    TOTAL_TIME=$((TOTAL_TIME + FUZZ_TIME))

    # Parse execution count from fuzzer output
    # libFuzzer prints stats like: stat::number_of_executed_units: 12345
    # or in the running output like: #12345 INITED/NEW/...
    execs=0
    stat_execs=$(grep -oP 'stat::number_of_executed_units:\s*\K\d+' "$FUZZ_LOG" 2>/dev/null | tail -1 || true)
    if [ -n "$stat_execs" ]; then
        execs=$stat_execs
    else
        # Fallback: parse the last #NNNN from running output
        last_count=$(grep -oP '#(\d+)' "$FUZZ_LOG" 2>/dev/null | tail -1 | tr -d '#' || true)
        if [ -n "$last_count" ]; then
            execs=$last_count
        fi
    fi

    exec_rate=$(grep -oP 'stat::average_exec_per_sec:\s*\K\d+' "$FUZZ_LOG" 2>/dev/null | tail -1 || true)
    exec_rate="${exec_rate:-0}"

    # Count crashes and divergences
    crashes=$(find "$h_crashes" -name 'crash-*' -o -name 'oom-*' 2>/dev/null | wc -l | tr -d ' ')
    divergences=$(grep -c "DIVERGENCE" "$FUZZ_LOG" 2>/dev/null || true)
    divergences="${divergences:-0}"

    corpus_size=$(find "$h_corpus" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')

    TOTAL_EXECS=$((TOTAL_EXECS + execs))
    TOTAL_CRASHES=$((TOTAL_CRASHES + crashes))
    TOTAL_DIVERGENCES=$((TOTAL_DIVERGENCES + divergences))

    # Report
    if [ "$crashes" -gt 0 ] || [ "$divergences" -gt 0 ]; then
        echo "  FAIL: ${crashes} crashes, ${divergences} divergences"
        OVERALL_RESULT=1
    else
        echo "  PASS: 0 crashes, 0 divergences"
    fi
    echo "  Executions: $execs | Rate: ${exec_rate}/s | Corpus: $corpus_size | Time: ${FUZZ_TIME}s"
    echo ""
done

# Summary
echo "=== Fuzz Summary ==="
echo "Total executions: $TOTAL_EXECS"
echo "Total crashes: $TOTAL_CRASHES"
echo "Total divergences: $TOTAL_DIVERGENCES"
echo "Total fuzz time: ${TOTAL_TIME}s (+ ${BUILD_TIME}s build)"
if [ "$OVERALL_RESULT" -eq 0 ]; then
    echo "Result: PASS"
else
    echo "Result: FAIL"
fi

exit "$OVERALL_RESULT"
