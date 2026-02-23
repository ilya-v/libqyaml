#!/usr/bin/env bash
#
# Automated extended fuzzing campaign script.
# Runs all fuzz harnesses for a configurable duration, seeds with all 3 required
# fuzz sources, and generates a structured markdown report in test-results/.
#
# Usage:
#   ./fuzz/run-campaign.sh [--duration MINUTES] [--forks N]
#
# Defaults:
#   --duration 30    (30 minutes total, split across harnesses)
#   --forks 3        (3 parallel libFuzzer workers per harness)
#
# Output:
#   test-results/fuzz-campaign-<commit>.md  (structured report)
#   test-output/campaign-<commit>/          (raw logs and artifacts)

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DURATION_MINUTES=30
FORKS=3

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration) DURATION_MINUTES="$2"; shift 2 ;;
        --forks)    FORKS="$2"; shift 2 ;;
        *)          echo "Unknown option: $1"; exit 2 ;;
    esac
done

COMMIT_FULL=$(cd "$REPO_ROOT" && git rev-parse HEAD)
COMMIT_SHORT=$(echo "$COMMIT_FULL" | cut -c1-8)
COMMIT_MSG=$(cd "$REPO_ROOT" && git log -1 --format='%s' HEAD)
TIMESTAMP=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

# Directories
CAMPAIGN_DIR="$REPO_ROOT/test-output/campaign-$COMMIT_SHORT"
REPORT_FILE="$REPO_ROOT/test-results/fuzz-campaign-$COMMIT_SHORT.md"
BUILD_DIR="$CAMPAIGN_DIR/build"
SEEDS_DIR="$REPO_ROOT/test-output/fuzz-seeds-combined"

mkdir -p "$CAMPAIGN_DIR" "$BUILD_DIR" "$SEEDS_DIR"

echo "=== Fuzzing Campaign ==="
echo "Commit: $COMMIT_SHORT ($COMMIT_MSG)"
echo "Duration: ${DURATION_MINUTES}m"
echo "Forks: $FORKS per harness"
echo ""

# -------------------------------------------------------------------
# 1. Collect seeds from all 3 required fuzz sources
# -------------------------------------------------------------------
collect_seeds() {
    local count_before
    count_before=$(find "$SEEDS_DIR" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
    if [ "$count_before" -gt 100 ]; then
        echo "Seeds already collected: $count_before files"
        return
    fi

    echo "Collecting seeds from external fuzz sources..."
    local src_count=0

    # Source 1: yaml-test-suite
    local yts_dir=""
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-test-suite/src" ] && yts_dir="$root/reference/yaml-test-suite/src" && break
    done
    if [ -n "$yts_dir" ]; then
        find "$yts_dir" -name 'in.yaml' -exec cp {} "$SEEDS_DIR/" \; 2>/dev/null
        src_count=$((src_count + 1))
        echo "  yaml-test-suite: found"
    else
        echo "  yaml-test-suite: NOT FOUND"
    fi

    # Source 2: yaml-test-data
    local ytd_dir=""
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-test-data" ] && ytd_dir="$root/reference/yaml-test-data" && break
    done
    if [ -n "$ytd_dir" ]; then
        find "$ytd_dir" \( -name '*.yaml' -o -name '*.yml' \) 2>/dev/null | while read f; do
            cp "$f" "$SEEDS_DIR/$(basename "$f").$(echo "$f" | md5sum | cut -c1-8)" 2>/dev/null
        done
        src_count=$((src_count + 1))
        echo "  yaml-test-data: found"
    else
        echo "  yaml-test-data: NOT FOUND"
    fi

    # Source 3: yaml-fuzz
    local yf_dir=""
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-fuzz" ] && yf_dir="$root/reference/yaml-fuzz" && break
    done
    if [ -n "$yf_dir" ]; then
        find "$yf_dir" \( -name '*.yaml' -o -name '*.yml' \) 2>/dev/null | while read f; do
            cp "$f" "$SEEDS_DIR/$(basename "$f").$(echo "$f" | md5sum | cut -c1-8)" 2>/dev/null
        done
        src_count=$((src_count + 1))
        echo "  yaml-fuzz: found"
    else
        echo "  yaml-fuzz: NOT FOUND"
    fi

    # Also add any existing fuzz/seeds from either tree
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        if [ -d "$root/fuzz/seeds" ]; then
            cp "$root/fuzz/seeds"/* "$SEEDS_DIR/" 2>/dev/null
            break
        fi
    done

    local count_after
    count_after=$(find "$SEEDS_DIR" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
    echo "  Total seeds: $count_after ($src_count/3 sources found)"
}

SEED_SOURCES=""
# When running from a worktree, reference/ may be in the main tree
MAIN_TREE_ROOT=$(cd "$REPO_ROOT" && git worktree list --porcelain 2>/dev/null | head -1 | sed 's/^worktree //' || echo "$REPO_ROOT")
check_seed_sources() {
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-test-suite/src" ] && SEED_SOURCES="${SEED_SOURCES}yaml-test-suite, " && break
    done
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-test-data" ] && SEED_SOURCES="${SEED_SOURCES}yaml-test-data, " && break
    done
    for root in "$REPO_ROOT" "$MAIN_TREE_ROOT"; do
        [ -d "$root/reference/yaml-fuzz" ] && SEED_SOURCES="${SEED_SOURCES}yaml-fuzz, " && break
    done
    SEED_SOURCES="${SEED_SOURCES%, }"  # trim trailing comma
}

collect_seeds
check_seed_sources
SEED_COUNT=$(find "$SEEDS_DIR" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')

# -------------------------------------------------------------------
# 2. Build library and harnesses
# -------------------------------------------------------------------
echo ""
echo "Building library with ASAN + fuzzer instrumentation..."
BUILD_START=$(date +%s)

cd "$BUILD_DIR"
cmake "$REPO_ROOT" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_C_FLAGS="-fsanitize=address,fuzzer-no-link -g" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQYAML_BUILD_TESTS=OFF \
    -DQYAML_BUILD_BENCHMARKS=OFF \
    > "$CAMPAIGN_DIR/build.log" 2>&1

cmake --build . -j"$(nproc)" >> "$CAMPAIGN_DIR/build.log" 2>&1
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed. See $CAMPAIGN_DIR/build.log"
    exit 2
fi

# Build each harness
HARNESSES=(
    "fuzz_scan:fuzz/fuzz_scan.c:"
    "fuzz_parse:fuzz/fuzz_parse.c:"
    "fuzz_load:fuzz/fuzz_load.c:"
    "fuzz_differential:fuzz/fuzz_differential.c:-ldl"
)
BUILT_HARNESSES=()

for entry in "${HARNESSES[@]}"; do
    IFS=':' read -r name src flags <<< "$entry"
    clang -fsanitize=fuzzer,address -g -I "$REPO_ROOT/include" \
        -o "$BUILD_DIR/$name" "$REPO_ROOT/$src" "$BUILD_DIR/libqyaml.a" $flags \
        >> "$CAMPAIGN_DIR/build.log" 2>&1
    if [ $? -eq 0 ]; then
        BUILT_HARNESSES+=("$name")
    else
        echo "  WARNING: Failed to build $name"
    fi
done

BUILD_END=$(date +%s)
BUILD_TIME=$((BUILD_END - BUILD_START))
echo "Build: ${BUILD_TIME}s (${#BUILT_HARNESSES[@]}/${#HARNESSES[@]} harnesses)"

# -------------------------------------------------------------------
# 3. Run each harness
# -------------------------------------------------------------------
TOTAL_DURATION_SEC=$((DURATION_MINUTES * 60))
HARNESS_COUNT=${#BUILT_HARNESSES[@]}
PER_HARNESS_SEC=$((TOTAL_DURATION_SEC / HARNESS_COUNT))

echo ""
echo "Running ${HARNESS_COUNT} harnesses for ${PER_HARNESS_SEC}s each (${DURATION_MINUTES}m total)"
echo ""

CAMPAIGN_START=$(date +%s)

declare -A H_EXECS H_CRASHES H_CORPUS H_COV H_TIME H_RESULT

for name in "${BUILT_HARNESSES[@]}"; do
    CORPUS_DIR="$CAMPAIGN_DIR/corpus-$name"
    CRASHES_DIR="$CAMPAIGN_DIR/crashes-$name"
    LOG_FILE="$CAMPAIGN_DIR/$name.log"
    mkdir -p "$CORPUS_DIR" "$CRASHES_DIR"

    echo "--- $name (${PER_HARNESS_SEC}s) ---"
    HSTART=$(date +%s)

    timeout $((PER_HARNESS_SEC + 10)) "$BUILD_DIR/$name" \
        "$CORPUS_DIR" "$SEEDS_DIR" \
        -artifact_prefix="$CRASHES_DIR/" \
        -max_total_time="$PER_HARNESS_SEC" \
        -max_len=8192 \
        -fork="$FORKS" \
        > "$LOG_FILE" 2>&1 || true

    HEND=$(date +%s)
    HDURATION=$((HEND - HSTART))

    # Parse stats from libFuzzer fork-mode output
    local_execs=$(grep -oP '#\K\d+(?=:)' "$LOG_FILE" | tail -1 || echo "0")
    local_corpus=$(grep -oP 'corp: \K\d+' "$LOG_FILE" | tail -1 || echo "0")
    local_cov=$(grep -oP 'cov: \K\d+' "$LOG_FILE" | tail -1 || echo "0")
    local_crashes=$(grep -oP 'crash: \K\d+' "$LOG_FILE" | tail -1 || echo "0")
    # Also count actual crash files
    local_crash_files=$(find "$CRASHES_DIR" -name 'crash-*' -type f 2>/dev/null | wc -l | tr -d ' ')

    if [ "$local_crash_files" -gt 0 ]; then
        local_result="FAIL"
    else
        local_result="PASS"
    fi

    H_EXECS[$name]="$local_execs"
    H_CRASHES[$name]="$local_crash_files"
    H_CORPUS[$name]="$local_corpus"
    H_COV[$name]="$local_cov"
    H_TIME[$name]="$HDURATION"
    H_RESULT[$name]="$local_result"

    echo "  $local_result: execs=$local_execs crashes=$local_crash_files corpus=$local_corpus cov=$local_cov time=${HDURATION}s"
done

CAMPAIGN_END=$(date +%s)
CAMPAIGN_DURATION=$((CAMPAIGN_END - CAMPAIGN_START))

# -------------------------------------------------------------------
# 4. Classify differential divergences (if any crashes)
# -------------------------------------------------------------------
DIFF_CRASHES_DIR="$CAMPAIGN_DIR/crashes-fuzz_differential"
DIFF_CRASH_COUNT=${H_CRASHES[fuzz_differential]:-0}
CLASSIFY_OUTPUT=""
TOTAL_DIVERGENT=0
PERM_COUNT=0 REST_COUNT=0 TOKTYPE_COUNT=0 SCALAR_COUNT=0
PERM_BOM=0 PERM_PCT=0 PERM_OTHER=0
REST_BOM=0 REST_PCT=0 REST_OTHER=0

if [ "$DIFF_CRASH_COUNT" -gt 0 ] && [ -f "$BUILD_DIR/../../../.worktree/strategic-tester/build-noasan/classify" ] 2>/dev/null; then
    # Try to use the existing classifier
    CLASSIFY_BIN="$BUILD_DIR/../../../.worktree/strategic-tester/build-noasan/classify"
elif [ "$DIFF_CRASH_COUNT" -gt 0 ] && [ -f "$REPO_ROOT/fuzz/classify_divergences.c" ]; then
    # Build classifier without ASAN
    echo ""
    echo "Building divergence classifier..."
    CLASSIFY_BUILD="$CAMPAIGN_DIR/build-classify"
    mkdir -p "$CLASSIFY_BUILD"
    cd "$CLASSIFY_BUILD"
    cmake "$REPO_ROOT" \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_BUILD_TYPE=Release \
        -DQYAML_BUILD_TESTS=OFF \
        -DQYAML_BUILD_BENCHMARKS=OFF \
        > /dev/null 2>&1
    cmake --build . -j"$(nproc)" > /dev/null 2>&1
    clang -O1 -g -I "$REPO_ROOT/include" \
        -o "$CLASSIFY_BUILD/classify" \
        "$REPO_ROOT/fuzz/classify_divergences.c" \
        "$CLASSIFY_BUILD/libqyaml.a" -ldl 2>/dev/null
    CLASSIFY_BIN="$CLASSIFY_BUILD/classify"
fi

if [ "$DIFF_CRASH_COUNT" -gt 0 ] && [ -n "${CLASSIFY_BIN:-}" ] && [ -x "${CLASSIFY_BIN}" ]; then
    echo ""
    echo "Classifying differential divergences..."
    CLASSIFY_OUTPUT=$("$CLASSIFY_BIN" "$DIFF_CRASHES_DIR" 2>&1)
    echo "$CLASSIFY_OUTPUT" > "$CAMPAIGN_DIR/classify-output.txt"

    TOTAL_DIVERGENT=$(echo "$CLASSIFY_OUTPUT" | grep -oP 'Total divergent:\s+\K\d+' || echo "0")
    PERM_COUNT=$(echo "$CLASSIFY_OUTPUT" | grep -A3 'PERMISSIVE' | head -1 | grep -oP '\d+' | head -1 || echo "0")
    REST_COUNT=$(echo "$CLASSIFY_OUTPUT" | grep -A3 'RESTRICTIVE' | head -1 | grep -oP '\d+' | head -1 || echo "0")
    TOKTYPE_COUNT=$(echo "$CLASSIFY_OUTPUT" | grep -A3 'TOKEN_TYPE' | head -1 | grep -oP '\d+' | head -1 || echo "0")
    SCALAR_COUNT=$(echo "$CLASSIFY_OUTPUT" | grep -oP 'SCALAR_VALUE.*?: \K\d+' || echo "0")
fi

# -------------------------------------------------------------------
# 5. Compute totals
# -------------------------------------------------------------------
TOTAL_EXECS=0
TOTAL_CRASHES=0
for name in "${BUILT_HARNESSES[@]}"; do
    TOTAL_EXECS=$((TOTAL_EXECS + ${H_EXECS[$name]:-0}))
    TOTAL_CRASHES=$((TOTAL_CRASHES + ${H_CRASHES[$name]:-0}))
done

# -------------------------------------------------------------------
# 6. Generate report
# -------------------------------------------------------------------
echo ""
echo "Generating report: $REPORT_FILE"

cat > "$REPORT_FILE" << REPORT_EOF
# Fuzzing Campaign Report — $COMMIT_SHORT

**Date:** $TIMESTAMP
**Commit:** $COMMIT_SHORT ($COMMIT_MSG)
**Full hash:** $COMMIT_FULL
**Duration:** ${DURATION_MINUTES} minutes (${CAMPAIGN_DURATION}s wall clock, ${BUILD_TIME}s build)
**Fuzzer:** libFuzzer (LLVM) with ASAN, fork=$FORKS per harness
**Seed sources:** $SEED_SOURCES ($SEED_COUNT seed files)
**Max input length:** 8192 bytes

## Summary

| Metric | Value |
|--------|-------|
| Harnesses run | ${HARNESS_COUNT} |
| Total inputs tested | $TOTAL_EXECS |
| Total crash artifacts | $TOTAL_CRASHES |
| Differential divergences | $TOTAL_DIVERGENT |
| Campaign duration | ${CAMPAIGN_DURATION}s |
| Build time | ${BUILD_TIME}s |

## Per-Harness Results

| Harness | Result | Executions | Crashes | Corpus | Coverage | Time |
|---------|--------|------------|---------|--------|----------|------|
REPORT_EOF

for name in "${BUILT_HARNESSES[@]}"; do
    echo "| $name | ${H_RESULT[$name]} | ${H_EXECS[$name]} | ${H_CRASHES[$name]} | ${H_CORPUS[$name]} | ${H_COV[$name]} | ${H_TIME[$name]}s |" >> "$REPORT_FILE"
done

cat >> "$REPORT_FILE" << REPORT_EOF

**Notes on per-harness results:**
- **fuzz_scan, fuzz_parse, fuzz_load**: Single-library crash-finding harnesses with ASAN. Any crash here is a memory safety bug.
- **fuzz_differential**: Compares libqyaml output against reference libyaml at scanner, parser, and loader levels. Any divergence (crash artifact) is a correctness bug. A single divergent input may diverge at multiple API levels (e.g., scanner divergence causes parser divergence too), so the divergence count below reflects unique divergent inputs, not total API-level failures.

REPORT_EOF

if [ "$TOTAL_DIVERGENT" -gt 0 ]; then
    cat >> "$REPORT_FILE" << REPORT_EOF
## Differential Divergence Classification

Total unique divergent inputs: $TOTAL_DIVERGENT

| Category | Count | Description |
|----------|-------|-------------|
| PERMISSIVE | $PERM_COUNT | libqyaml accepts, reference rejects |
| RESTRICTIVE | $REST_COUNT | libqyaml rejects, reference accepts |
| TOKEN_TYPE | $TOKTYPE_COUNT | Both scan successfully but produce different token types |
| SCALAR_VALUE | $SCALAR_COUNT | Both scan successfully but produce different scalar content |

**Overlap note:** Each input is classified into exactly one category based on the first divergence detected at the scanner level. There is no double-counting — total = PERMISSIVE + RESTRICTIVE + TOKEN_TYPE + SCALAR_VALUE.

REPORT_EOF

    if [ -f "$CAMPAIGN_DIR/classify-output.txt" ]; then
        # Extract the detailed "Other" section
        OTHER_SECTION=$(sed -n '/=== DETAILED:/,$ p' "$CAMPAIGN_DIR/classify-output.txt" | head -80)
        if [ -n "$OTHER_SECTION" ]; then
            cat >> "$REPORT_FILE" << REPORT_EOF
### Detailed Non-BOM, Non-% Divergences (first 20)

\`\`\`
$(echo "$OTHER_SECTION" | head -80)
\`\`\`

REPORT_EOF
        fi
    fi
fi

cat >> "$REPORT_FILE" << REPORT_EOF
## Memory Safety

REPORT_EOF

SAFETY_ISSUES=0
for name in fuzz_scan fuzz_parse fuzz_load; do
    crashes=${H_CRASHES[$name]:-0}
    if [ "$crashes" -gt 0 ]; then
        echo "- **$name**: $crashes ASAN crashes detected — INVESTIGATE IMMEDIATELY" >> "$REPORT_FILE"
        SAFETY_ISSUES=$((SAFETY_ISSUES + crashes))
    fi
done

if [ "$SAFETY_ISSUES" -eq 0 ]; then
    SAFETY_EXECS=0
    for name in fuzz_scan fuzz_parse fuzz_load; do
        SAFETY_EXECS=$((SAFETY_EXECS + ${H_EXECS[$name]:-0}))
    done
    echo "No memory safety issues found across $SAFETY_EXECS ASAN-instrumented executions." >> "$REPORT_FILE"
fi

cat >> "$REPORT_FILE" << REPORT_EOF

## Artifacts

- Raw logs: \`test-output/campaign-$COMMIT_SHORT/\`
- Crash artifacts: \`test-output/campaign-$COMMIT_SHORT/crashes-*/\`
- Corpus: \`test-output/campaign-$COMMIT_SHORT/corpus-*/\`
- Classification: \`test-output/campaign-$COMMIT_SHORT/classify-output.txt\`
REPORT_EOF

echo ""
echo "=== Campaign Complete ==="
echo "Duration: ${CAMPAIGN_DURATION}s"
echo "Total executions: $TOTAL_EXECS"
echo "Total crashes: $TOTAL_CRASHES"
echo "Divergences: $TOTAL_DIVERGENT"
echo "Report: $REPORT_FILE"
