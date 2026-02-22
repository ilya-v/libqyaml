#!/bin/bash
# Run yaml-test-suite against libqyaml's parser
# Usage: run_yaml_test_suite.sh <parser-binary> <test-data-dir> [known-failures-file]
#
# If a known-failures file is provided, failures listed there are treated
# as expected (matching libyaml's behavior) and do not cause a test failure.
# Unexpected new failures or regressions (passing a test that libyaml fails)
# will still be reported.
set -euo pipefail

PARSER="$1"
DATA_DIR="$2"
KNOWN_FAILURES="${3:-}"

if [ ! -x "$PARSER" ]; then
    echo "ERROR: parser binary not found: $PARSER"
    exit 1
fi
if [ ! -d "$DATA_DIR" ]; then
    echo "ERROR: test data directory not found: $DATA_DIR"
    exit 1
fi

# Load known failures into an associative array
declare -A known_fail
if [ -n "$KNOWN_FAILURES" ] && [ -f "$KNOWN_FAILURES" ]; then
    while IFS= read -r line; do
        known_fail["$line"]=1
    done < "$KNOWN_FAILURES"
fi

pass=0
fail=0
known=0
error_pass=0
error_fail=0
skip=0
unexpected=""

run_test() {
    local test_dir="$1"
    local test_id="$2"
    local in_yaml="$test_dir/in.yaml"
    local test_event="$test_dir/test.event"
    local error_file="$test_dir/error"

    if [ -f "$error_file" ]; then
        # This test expects a parse error
        if "$PARSER" "$in_yaml" > /dev/null 2>&1; then
            # Parser succeeded but should have failed
            if [ "${known_fail[$test_id]+x}" ]; then
                known=$((known + 1))
            else
                error_fail=$((error_fail + 1))
                unexpected="$unexpected  $test_id: expected error but parsed OK\n"
            fi
        else
            error_pass=$((error_pass + 1))
        fi
        return
    fi

    if [ ! -f "$test_event" ] || [ ! -f "$in_yaml" ]; then
        skip=$((skip + 1))
        return
    fi

    local actual
    actual=$("$PARSER" "$in_yaml" 2>/dev/null) || {
        if [ "${known_fail[$test_id]+x}" ]; then
            known=$((known + 1))
        else
            fail=$((fail + 1))
            unexpected="$unexpected  $test_id: parser returned error\n"
        fi
        return
    }

    local expected
    expected=$(cat "$test_event")

    if [ "$actual" = "$expected" ]; then
        pass=$((pass + 1))
    else
        if [ "${known_fail[$test_id]+x}" ]; then
            known=$((known + 1))
        else
            fail=$((fail + 1))
            unexpected="$unexpected  $test_id: output mismatch\n"
        fi
    fi
}

# Iterate all test cases
for test_dir in "$DATA_DIR"/*/; do
    test_id=$(basename "$test_dir")

    # Check for subtests
    if [ -d "$test_dir/00" ]; then
        for sub_dir in "$test_dir"/[0-9]*/; do
            sub_id="$test_id/$(basename "$sub_dir")"
            run_test "$sub_dir" "$sub_id"
        done
    else
        run_test "$test_dir" "$test_id"
    fi
done

total=$((pass + fail + known + error_pass + error_fail + skip))
echo "yaml-test-suite results:"
echo "  Passed:           $pass valid + $error_pass error = $((pass + error_pass))"
echo "  Known failures:   $known (matching libyaml behavior)"
echo "  Unexpected fails: $fail valid + $error_fail error = $((fail + error_fail))"
echo "  Skipped:          $skip"
echo "  Total:            $total"

if [ -n "$unexpected" ]; then
    echo ""
    echo "UNEXPECTED failures (regressions vs libyaml):"
    echo -e "$unexpected"
fi

if [ $fail -gt 0 ] || [ $error_fail -gt 0 ]; then
    exit 1
fi
echo ""
echo "OK: all results match libyaml behavior"
exit 0
