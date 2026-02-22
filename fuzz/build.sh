#!/bin/bash
# Build fuzz harnesses using clang with libFuzzer and AddressSanitizer
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

CC="${CC:-clang}"
CFLAGS="-fsanitize=fuzzer,address -I${PROJECT_DIR}/include -I${PROJECT_DIR}/build/include -O1 -g -DHAVE_CONFIG_H"

SOURCES="${PROJECT_DIR}/src/api.c ${PROJECT_DIR}/src/reader.c ${PROJECT_DIR}/src/scanner.c \
         ${PROJECT_DIR}/src/parser.c ${PROJECT_DIR}/src/loader.c ${PROJECT_DIR}/src/emitter.c \
         ${PROJECT_DIR}/src/dumper.c ${PROJECT_DIR}/src/writer.c"

echo "Building fuzz harnesses..."

$CC $CFLAGS "${SCRIPT_DIR}/fuzz_scan.c" $SOURCES -o "${SCRIPT_DIR}/fuzz_scan"
echo "  Built fuzz_scan"

$CC $CFLAGS "${SCRIPT_DIR}/fuzz_parse.c" $SOURCES -o "${SCRIPT_DIR}/fuzz_parse"
echo "  Built fuzz_parse"

$CC $CFLAGS "${SCRIPT_DIR}/fuzz_load.c" $SOURCES -o "${SCRIPT_DIR}/fuzz_load"
echo "  Built fuzz_load"

$CC $CFLAGS "${SCRIPT_DIR}/fuzz_parse_structured.c" $SOURCES -o "${SCRIPT_DIR}/fuzz_parse_structured"
echo "  Built fuzz_parse_structured (structure-aware)"

echo "Done. Run with: ./fuzz/<harness> fuzz/seeds/ -max_total_time=300"
