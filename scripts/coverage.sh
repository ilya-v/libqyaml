#!/bin/bash
# Run tests and generate code coverage report
# Usage: ./scripts/coverage.sh

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-coverage"

echo "=== Building with coverage instrumentation ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Debug -DQYAML_BUILD_TESTS=ON -DQYAML_COVERAGE=ON "$PROJECT_DIR" 2>&1 | tail -1
make -j4 2>&1 | tail -1

echo ""
echo "=== Running tests ==="
ctest -j4 2>&1

echo ""
echo "=== Generating coverage report ==="
lcov --capture --directory CMakeFiles/qyaml.dir/src --output-file coverage.info --gcov-tool gcov 2>&1 | tail -3

echo ""
echo "=== Per-file coverage ==="
lcov --list coverage.info 2>&1

echo ""
echo "Coverage data saved to: $BUILD_DIR/coverage.info"
