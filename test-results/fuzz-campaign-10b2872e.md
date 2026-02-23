# Fuzzing Campaign Report — 10b2872e

**Date:** 2026-02-23T15:07:54Z
**Commit:** 10b2872e (test: Add error divergence detection to differential harness and classifier)
**Full hash:** 10b2872e9410886f9fa5adfee45829d2cf3d08de
**Duration:** 10 minutes (482s wall clock, 5s build)
**Fuzzer:** libFuzzer (LLVM) with ASAN, fork=3 per harness
**Seed sources:** yaml-test-suite, yaml-test-data, yaml-fuzz (706 seed files)
**Max input length:** 8192 bytes

## Summary

| Metric | Value |
|--------|-------|
| Harnesses run | 4 |
| Total inputs tested | 13714170 |
| Total crash artifacts | 14 |
| Differential divergences (output) | 7 |
| Differential divergences (error) | 0 |
| Campaign duration | 482s |
| Build time | 5s |

## Per-Harness Results

| Harness | Result | Executions | Crashes | Corpus | Coverage | Time |
|---------|--------|------------|---------|--------|----------|------|
| fuzz_scan | PASS | 4937897 | 0 | 2490 | 1822 | 159s |
| fuzz_parse | PASS | 4818269 | 0 | 2504 | 2243 | 159s |
| fuzz_load | PASS | 3957982 | 0 | 2679 | 2574 | 159s |
| fuzz_differential | FAIL | 22 | 14 | 19 | 1245 | 5s |

**Notes on per-harness results:**
- **fuzz_scan, fuzz_parse, fuzz_load**: Single-library crash-finding harnesses with ASAN. Any crash here is a memory safety bug.
- **fuzz_differential**: Compares libqyaml output against reference libyaml at scanner, parser, and loader levels. Any divergence (crash artifact) is a correctness bug. A single divergent input may diverge at multiple API levels (e.g., scanner divergence causes parser divergence too), so the divergence count below reflects unique divergent inputs, not total API-level failures.

## Differential Divergence Classification

Total unique divergent inputs: 7

| Category | Count | Description |
|----------|-------|-------------|
| PERMISSIVE | 2 | libqyaml accepts, reference rejects |
| RESTRICTIVE | 5 | libqyaml rejects, reference accepts |
| TOKEN_TYPE | 0 | Both scan successfully but produce different token types |
| SCALAR_VALUE | 0 | Both scan successfully but produce different scalar content |
| ERROR_DIVERGENCE | 0 | Both fail but with different error codes or descriptions |

**Overlap note:** Each input is classified into exactly one category based on the first divergence detected at the scanner level. There is no double-counting — total = PERMISSIVE + RESTRICTIVE + TOKEN_TYPE + SCALAR_VALUE + ERROR_DIVERGENCE.

**Error divergence detail:** When both libraries reject an input, but they report different error codes or error messages, this is an error divergence — distinct from cosmetic differences (same error code and message, different position).

### Detailed Non-BOM, Non-% Divergences (first 20)

```
=== DETAILED: Non-BOM, Non-% Divergences ===

[1] crash-3147ef9ed78d2e0d046ee6144715f906e84996fc (PERMISSIVE) tok#5, 16 bytes
  Ref: found a tab character that violates indentation at 1:0
  Input: foo: 1\n\t\nbar: 2\n

[2] crash-aeeaefdddf1ab3980fc0b4c37f2ecffb7d8033c4 (RESTRICTIVE) tok#5, 19 bytes
  Our: could not find expected ':' at 3:0
  Input: foo:\n  bar\ninvalid\n

[3] crash-0e245348803b11a2b03d4871fb0aa3fc05fc0a16 (PERMISSIVE) tok#9, 20 bytes
  Ref: found a tab character that violates indentation at 2:2
  Input: foo:\n  a: 1\n  \tb: 2\n

[4] crash-a4191e139d2aa00c48e9d23c1f66e6d329429ecf (RESTRICTIVE) tok#5, 24 bytes
  Our: could not find expected ':' at 3:0
  Input: - item1\n- item2\ninvalid\n

[5] crash-84cf6e20b6eb792660894fd85d4206c06969213d (RESTRICTIVE) tok#9, 28 bytes
  Our: could not find expected ':' at 4:0
  Input: key:\n - bar\n - baz\n invalid\n

[6] crash-9b6a2f0e4e8b7e37c9953fc4bb4d4174b88be5cf (RESTRICTIVE) tok#9, 31 bytes
  Our: could not find expected ':' at 4:0
  Input: key:\n - item1\n - item2\ninvalid\n

[7] crash-4c4b90969fd2ce2338fab0c61eda2a89bdceb84e (RESTRICTIVE) tok#7, 68 bytes
  Our: could not find expected ':' at 3:0
  Input: sequence: !!seq\n- entry\n-! !seq\nc - nested\nmapping: !!map\n foo: bar\n
```

## Memory Safety

No memory safety issues found across 13714148 ASAN-instrumented executions.

## Artifacts

- Raw logs: `test-output/campaign-10b2872e/`
- Crash artifacts: `test-output/campaign-10b2872e/crashes-*/`
- Corpus: `test-output/campaign-10b2872e/corpus-*/`
- Classification: `test-output/campaign-10b2872e/classify-output.txt`
