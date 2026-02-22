#!/bin/bash
# Benchmark comparison: libqyaml vs libyaml vs libfyaml
# Uses command-line parser tools to measure parsing throughput on real files.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Parser binaries
QYAML_PARSER="${PROJECT_DIR}/build/tests/conformance-run-parser-test-suite"
LIBYAML_PARSER=""
FYAML_PARSER=""

# Build libyaml parser if reference exists
if [ -f "${PROJECT_DIR}/reference/libyaml/build/libyaml.a" ]; then
    cc -O3 -march=native -DYAML_DECLARE_STATIC \
        -I"${PROJECT_DIR}/reference/libyaml/include" \
        "${PROJECT_DIR}/reference/libyaml/tests/run-parser-test-suite.c" \
        "${PROJECT_DIR}/reference/libyaml/build/libyaml.a" \
        -o /tmp/bench-libyaml-parser 2>/dev/null
    LIBYAML_PARSER="/tmp/bench-libyaml-parser"
fi

# Check for libfyaml
if command -v fy-dump &>/dev/null; then
    FYAML_PARSER="fy-dump"
fi

# Generate test files
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

generate_k8s() {
    local file="$1"
    local count="$2"
    echo "apiVersion: v1" > "$file"
    echo "kind: ServiceList" >> "$file"
    echo "items:" >> "$file"
    for i in $(seq 1 "$count"); do
        cat >> "$file" << ITEM
  - apiVersion: v1
    kind: Service
    metadata:
      name: service-$i
      namespace: default
      labels:
        app: myapp-$i
        version: v1.0.$i
        tier: backend
      annotations:
        description: "Service number $i"
    spec:
      type: ClusterIP
      selector:
        app: myapp-$i
      ports:
        - name: http
          port: $((8000 + i))
          targetPort: 8080
          protocol: TCP
        - name: https
          port: $((8100 + i))
          targetPort: 8443
          protocol: TCP
ITEM
    done
}

generate_mapping() {
    local file="$1"
    local count="$2"
    > "$file"
    for i in $(seq 1 "$count"); do
        printf "key_%06d: value_%06d\n" "$i" "$i" >> "$file"
    done
}

generate_flow() {
    local file="$1"
    local count="$2"
    printf "[" > "$file"
    for i in $(seq 1 "$count"); do
        if [ "$i" -gt 1 ]; then printf ", " >> "$file"; fi
        printf "%d" "$i" >> "$file"
    done
    printf "]\n" >> "$file"
}

generate_nested() {
    local file="$1"
    local depth="$2"
    > "$file"
    local indent=""
    for i in $(seq 1 "$depth"); do
        printf "%slevel_%d:\n" "$indent" "$i" >> "$file"
        indent="$indent  "
    done
    printf "%svalue: leaf\n" "$indent" >> "$file"
}

echo "Generating test files..."
generate_k8s "$TMPDIR/k8s_small.yaml" 10
generate_k8s "$TMPDIR/k8s_medium.yaml" 100
generate_k8s "$TMPDIR/k8s_large.yaml" 500
generate_mapping "$TMPDIR/mapping.yaml" 10000
generate_flow "$TMPDIR/flow.yaml" 10000
generate_nested "$TMPDIR/nested.yaml" 200

echo ""
echo "Test files:"
for f in "$TMPDIR"/*.yaml; do
    size=$(wc -c < "$f")
    name=$(basename "$f")
    printf "  %-25s %8d bytes (%s)\n" "$name" "$size" "$(numfmt --to=iec $size 2>/dev/null || echo "${size}B")"
done

# Benchmark function: run parser N times and report throughput
bench_parser() {
    local name="$1"
    local parser="$2"
    local file="$3"
    local iters="$4"
    local extra_args="${5:-}"
    local size=$(wc -c < "$file")

    if [ -z "$parser" ]; then
        printf "  %-20s  (not available)\n" "$name"
        return
    fi

    # Warmup
    for i in 1 2 3; do
        $parser $extra_args "$file" > /dev/null 2>&1 || true
    done

    # Timed run
    local start=$(date +%s%N)
    for i in $(seq 1 "$iters"); do
        $parser $extra_args "$file" > /dev/null 2>&1 || true
    done
    local end=$(date +%s%N)
    local elapsed_ns=$(( end - start ))
    local elapsed_ms=$(( elapsed_ns / 1000000 ))

    if [ $elapsed_ns -gt 0 ]; then
        local throughput_kbs=$(( size * iters * 1000000000 / elapsed_ns / 1024 ))
        local latency_us=$(( elapsed_ns / iters / 1000 ))
        printf "  %-20s  %6d KB/s  %8d us/iter  (%d iters, %d ms)\n" \
            "$name" "$throughput_kbs" "$latency_us" "$iters" "$elapsed_ms"
    else
        printf "  %-20s  too fast to measure\n" "$name"
    fi
}

echo ""
echo "========================================="
echo "  YAML Parser Benchmark Comparison"
echo "========================================="
echo ""

for yaml_file in "$TMPDIR"/k8s_small.yaml "$TMPDIR"/k8s_medium.yaml "$TMPDIR"/k8s_large.yaml \
                  "$TMPDIR"/mapping.yaml "$TMPDIR"/flow.yaml "$TMPDIR"/nested.yaml; do
    name=$(basename "$yaml_file" .yaml)
    size=$(wc -c < "$yaml_file")

    # Choose iteration count based on file size
    if [ "$size" -lt 10000 ]; then
        iters=500
    elif [ "$size" -lt 100000 ]; then
        iters=200
    elif [ "$size" -lt 500000 ]; then
        iters=50
    else
        iters=20
    fi

    echo "$name ($(numfmt --to=iec $size 2>/dev/null || echo "${size}B")):"
    bench_parser "libqyaml" "$QYAML_PARSER" "$yaml_file" "$iters"
    bench_parser "libyaml" "$LIBYAML_PARSER" "$yaml_file" "$iters"
    bench_parser "libfyaml" "$FYAML_PARSER" "$yaml_file" "$iters" "--dump"
    echo ""
done

echo "Note: Command-line benchmarks include process startup and I/O overhead."
echo "      The programmatic bench-qyaml/bench-libyaml comparison is more precise"
echo "      for measuring pure parsing throughput."
