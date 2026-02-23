# Differential Fuzzing Campaign Report — bac0cb92

**Date:** 2026-02-23
**Commit:** bac0cb92 (test: Add empty complex key regression tests for loader bug)
**Duration:** ~60 seconds fork-mode fuzzing + manual analysis
**Fuzzer:** libFuzzer (LLVM 21.1.8) with ASAN, fork=4
**Harness:** fuzz/fuzz_differential.c (dlopen reference libyaml-0.so.2 at runtime)

## Summary

| Metric | Value |
|--------|-------|
| Total corpus inputs tested | ~82 (fork mode, 4 workers) |
| Divergent inputs found | 97 |
| Scanner divergences | 85 |
| Parser divergences | 60 |
| Loader-only divergences | 11 |
| ASAN errors | 0 |
| Execution rate | ~10 exec/s (slow due to 3x API comparison per input) |

## Divergence Categories

### 1. Scanner: libqyaml too permissive (50 inputs)

libqyaml accepts inputs that libyaml rejects with "found character that cannot start any token".

**Pattern:** The `%` character appears in non-directive positions (inside flow collections, after other tokens). libyaml correctly rejects these; libqyaml does not.

**Smallest reproducer:** `?$[: [%o` (8 bytes)
- Token #6: libqyaml succeeds, libyaml fails

### 2. Scanner: libqyaml too restrictive (21 inputs)

libqyaml rejects inputs that libyaml accepts.

**Pattern:** UTF-8 BOM (`\xef\xbb\xbf`) appears between documents or in mid-stream positions. libqyaml fails with "could not find expected ':'" where libyaml continues scanning.

**Smallest reproducer:** `---\ndoc1: e1\n\xef\xbb\xbf---\ndvague1TA\n` (30 bytes)
- Token #7: libqyaml fails, libyaml succeeds

### 3. Scanner: Different token types (14 inputs)

Both libraries scan successfully but produce different token sequences.

**Pattern:** BOM in mid-stream causes token reordering (e.g., libqyaml emits SCALAR where libyaml emits BLOCK_ENTRY).

**Smallest reproducer:** `- iue\n\n ne\xef\xbb\xbf\n- nullul_v\n` (25 bytes)
- Token #4: libqyaml emits SCALAR, libyaml emits BLOCK_ENTRY

### 4. Loader: Empty complex key failure (11 inputs)

Scanner and parser produce identical output to libyaml, but the loader fails.

**Pattern:** Input starts with `?` (explicit complex key with no content). Parser correctly emits MAPPING_START, SCALAR(empty), SCALAR(empty), MAPPING_END, but the loader fails with "did not find expected node content".

**Smallest reproducer:** `?` (1 byte)
- load returns 0 (error), libyaml returns 1 (success)

## Root Cause Analysis

### Bug 1: % character handling in scanner
The scanner's fetch_next_token logic does not correctly reject `%` when it appears in non-directive positions (e.g., inside flow collections). This makes libqyaml more permissive than libyaml, violating the compatibility requirement.

### Bug 2: BOM handling in mid-stream
The scanner does not correctly handle UTF-8 BOM when it appears between documents or in mid-content positions. This affects both acceptance (some valid inputs rejected) and token ordering.

### Bug 3: Loader empty complex key
The loader's `yaml_parser_load` does not handle the case where parser events include empty scalars from explicit complex keys (`?` without content). The parser correctly emits the events but the loader fails to process them.

## Reproducer Artifacts

Saved to `test-output/diff-fuzz-artifacts/minimized/`:
- `loader-empty-complex-key-1byte.bin` — 1 byte (`?`)
- `permissive-scan-8bytes.bin` — 8 bytes (`?$[: [%o`)
- `restrictive-scan-30bytes.bin` — 30 bytes (BOM between docs)
- `typediff-scan-25bytes.bin` — 25 bytes (BOM mid-stream)

Full crash corpus (97 files) at `test-output/diff-fuzz-crashes/`.

## Recommendations

1. **CRITICAL:** Fix loader empty complex key handling (Bug 3) — this affects valid, common YAML constructs
2. **HIGH:** Fix BOM mid-stream handling (Bug 2) — affects multi-document streams
3. **MEDIUM:** Fix % character permissiveness (Bug 1) — makes libqyaml accept invalid YAML

## Next Steps

- Continue differential fuzzing with longer campaigns after bugs are fixed
- Seed the fuzzer with the YAML test suite corpus for broader coverage
- Run sustained single-library fuzzing in parallel for crash/memory safety testing
