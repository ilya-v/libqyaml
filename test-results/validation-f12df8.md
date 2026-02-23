# Validation Report: f12df8da

**Commit:** f12df8da — fix: Use correct parser state in block sequence batch loader fallback
**Date:** 2026-02-23
**Tested by:** tester-3

## Change Summary

One-line fix in `src/loader.c`: changed parser state in the block sequence batch loader fallback path from `YAML_PARSE_BLOCK_NODE_OR_INDENTLESS_SEQUENCE_STATE` to `YAML_PARSE_BLOCK_NODE_STATE`. This corrects silent data corruption where the wrong parser state could cause incorrect parsing of block sequence items.

## Build

| Build | Status |
|-------|--------|
| Release (-O2) | PASS |
| ASAN+UBSAN (clang) | PASS |

## Unit Tests (Release)

- **Result:** 40/40 passed, 0 failed
- **Total time:** 0.67s

## Differential Tests

- **test-differential-libyaml:** 294/294 passed — zero divergences on curated inputs
- **Differential fuzzer:** 1 divergence (pre-existing, see Known Issues)

## ASAN+UBSAN

- **Result:** 39/39 passed, 0 errors
- Total time: 12.4s

## Quick Fuzz

| Harness | Executions | Crashes | Divergences | Time |
|---------|-----------|---------|-------------|------|
| fuzz_scan | 231,091 | 0 | 0 | 7s |
| fuzz_parse | 343,285 | 0 | 0 | 7s |
| fuzz_load | 271,514 | 0 | 0 | 7s |
| fuzz_differential | 16 | 1 | 1 | <1s |
| **Total** | **845,906** | **1** | **1** | **21s** |

The differential fuzzer crash is the pre-existing scanner status divergence on `foo:\n  bar\ninvalid\n` (missing SCALAR token before error), already reported in validation-5953a6.md. Not introduced by this commit.

## Benchmarks

| Workload | API | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|----------|-----|--------------|----------------|---------|
| Kubernetes (50 KB) | scan | 305.4 | 98.1 | 3.11x |
| Kubernetes (50 KB) | parse | 360.6 | 98.5 | 3.66x |
| Kubernetes (50 KB) | load | 349.1 | 73.1 | 4.78x |
| Mapping (244 KB) | scan | 670.4 | 111.7 | 6.00x |
| Mapping (244 KB) | parse | 585.7 | 113.6 | 5.16x |
| Mapping (244 KB) | load | 673.3 | 77.5 | 8.69x |
| Flow (57 KB) | scan | 259.6 | 91.8 | 2.83x |
| Flow (57 KB) | parse | 248.5 | 81.1 | 3.06x |
| Flow (57 KB) | load | 275.9 | 53.6 | 5.15x |
| Small (35 B) | scan | 96.2 | 57.3 | 1.68x |
| Small (35 B) | parse | 74.0 | 42.1 | 1.76x |
| Small (35 B) | load | 48.4 | 26.4 | 1.83x |

**Performance range:** 1.68x - 8.69x vs libyaml
**Peak:** Mapping load at 673.3 MB/s (8.69x)

## Known Issues (pre-existing, not introduced by this commit)

- Scanner status divergence on `foo:\n  bar\ninvalid\n` — our scanner fails to emit a value scalar that libyaml produces before the error. Reported to worker in previous validation cycle.

## Summary

- **Unit tests:** PASS (40/40)
- **ASAN+UBSAN:** PASS (39/39 clean)
- **Differential (curated):** PASS (294/294)
- **Differential (fuzz):** 1 pre-existing divergence (not new)
- **Benchmarks:** 1.68x - 8.69x vs libyaml
- **No new bugs introduced** by this commit. The batch loader state fix is clean.

**Overall: PASS** (excluding pre-existing known issue)

## Total Validation Time

~95s (build + unit tests + ASAN + differential + fuzz + benchmarks)
