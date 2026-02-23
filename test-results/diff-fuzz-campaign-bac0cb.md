# Differential Fuzzing Campaign Report — bac0cb92

**Date:** 2026-02-23
**Commit:** bac0cb92 (test: Add empty complex key regression tests for loader bug)
**Duration:** ~60 seconds fork-mode fuzzing + manual analysis
**Fuzzer:** libFuzzer (LLVM 21.1.8) with ASAN, fork=4
**Harness:** fuzz/fuzz_differential.c (dlopen reference libyaml-0.so.2 at runtime)
**Seed sources:** yaml-test-suite only (yaml-test-data and yaml-fuzz not yet integrated at time of run)

## Summary

| Metric | Value |
|--------|-------|
| Initial seed corpus | ~82 files |
| Total inputs tested | ~1,500 (seeds + libFuzzer mutations) |
| Unique divergent inputs saved | 97 |
| ASAN errors | 0 |
| Execution rate | ~10 exec/s (slow due to 3x API comparison per input) |

**Note on category counts:** Each divergent input is tested at all 3 API levels (scanner, parser, loader) sequentially. A single input can diverge at multiple levels — e.g., a scanner divergence usually also causes a parser divergence. The counts below reflect how many inputs diverge at each level, NOT distinct bugs:

| API Level | Divergent Inputs | Notes |
|-----------|-----------------|-------|
| Scanner | 85 | 85 inputs diverge at scanner level |
| Parser | 60 | Subset of scanner divergences that also affect parser |
| Loader-only | 11 | Diverge at loader level but NOT at scanner/parser |

Total unique divergent inputs = 85 (scanner) + 11 (loader-only) = 96 distinct inputs (97 including one edge case with an off-by-one in counting).

## Divergence Categories

### 1. Scanner: libqyaml too permissive (50 inputs)

libqyaml accepts inputs that libyaml rejects with "found character that cannot start any token".

**Pattern:** The `%` character appears in non-directive positions (inside flow collections, after other tokens). libyaml correctly rejects these; libqyaml does not.

**Smallest reproducer:** `?$[: [%o` (8 bytes)
- Token #6: libqyaml succeeds, libyaml fails

**Status:** FIXED in commit 8f2c42d8.

### 2. Scanner: libqyaml too restrictive (21 inputs)

libqyaml rejects inputs that libyaml accepts.

**Pattern:** UTF-8 BOM (`\xef\xbb\xbf`) appears between documents or in mid-stream positions. libqyaml fails with "could not find expected ':'" where libyaml continues scanning.

**Smallest reproducer:** `---\ndoc1: e1\n\xef\xbb\xbf---\ndvague1TA\n` (30 bytes)
- Token #7: libqyaml fails, libyaml succeeds

**Status:** Open.

### 3. Scanner: Different token types (14 inputs)

Both libraries scan successfully but produce different token sequences.

**Pattern:** BOM in mid-stream causes token reordering (e.g., libqyaml emits SCALAR where libyaml emits BLOCK_ENTRY).

**Smallest reproducer:** `- iue\n\n ne\xef\xbb\xbf\n- nullul_v\n` (25 bytes)
- Token #4: libqyaml emits SCALAR, libyaml emits BLOCK_ENTRY

**Status:** Open (related to BOM handling, Bug 2).

### 4. Loader: Empty complex key failure (11 inputs)

Scanner and parser produce identical output to libyaml, but the loader fails.

**Pattern:** Input starts with `?` (explicit complex key with no content). Parser correctly emits MAPPING_START, SCALAR(empty), SCALAR(empty), MAPPING_END, but the loader fails with "did not find expected node content".

**Smallest reproducer:** `?` (1 byte)
- load returns 0 (error), libyaml returns 1 (success)

**Status:** FIXED in commit 16fae05c.

## Root Cause Analysis

### Bug 1: % character handling in scanner (FIXED)
The scanner's fetch_next_token logic did not correctly reject `%` when it appeared in non-directive positions. Fixed in 8f2c42d8.

### Bug 2: BOM handling in mid-stream (Open)
The scanner does not correctly handle UTF-8 BOM when it appears between documents or in mid-content positions. This affects both acceptance (some valid inputs rejected) and token ordering.

### Bug 3: Loader empty complex key (FIXED)
The loader's `yaml_parser_load` did not handle the case where parser events include empty scalars from explicit complex keys. Fixed in 16fae05c.

## Limitations

This was a short (~60 second) smoke-test run, not a sustained campaign. Only one seed source (yaml-test-suite) was used. A proper extended campaign with all 3 seed sources was run subsequently — see `diff-fuzz-campaign-e3cc40.md`.
