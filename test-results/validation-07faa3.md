# Validation Report: 07faa3aa

**Commit:** 07faa3aa — fix: Reject invalid flow sequences in batch loader and improve tab error message
**Date:** 2026-02-23
**Tested by:** tester-3

## Build

| Build | Status |
|-------|--------|
| Release (-O2) | PASS |
| ASAN+UBSAN (clang) | PASS |

## Unit Tests (Release)

- **Result:** 40/41 passed, 0 failed, 1 hung (coverage-targets — known issue, excluded)
- **Total time:** ~5s (excluding coverage-targets)
- All core tests pass: scanner, parser, loader, emitter, reader, differential, edge-cases, conformance, stress, truncation, deep-nesting, OOM, guard-page, alloc-tracker, roundtrip, regression-fuzz, resource-constrained, coverage-gaps, loader-error-propagation

## Differential Tests

- **test-differential-libyaml:** 294/294 passed — zero divergences on curated inputs
- **Differential fuzzer:** DIVERGENCE FOUND (see below)

## ASAN+UBSAN

- **Result:** 39/39 passed, 0 errors
- All tests clean under AddressSanitizer and UndefinedBehaviorSanitizer
- Total time: 25.0s

## Quick Fuzz

| Harness | Executions | Crashes | Divergences | Time |
|---------|-----------|---------|-------------|------|
| fuzz_scan | 380,268 | 0 | 0 | 8s |
| fuzz_parse | 401,544 | 0 | 0 | 7s |
| fuzz_load | 322,882 | 0 | 0 | 7s |
| fuzz_differential | 16 | 1 | 1 | <1s |
| **Total** | **1,104,710** | **1** | **1** | **22s** |

### CRITICAL: Differential Divergence

**Input:** `0x2d 0x09` (dash followed by tab, "-\t")
**Level:** Scanner error message
**Ours:** `found a tab character that violates indentation`
**Ref:** `found character that cannot start any token`

This is the tab error message change introduced in this commit. The error message differs from libyaml's output, which constitutes a divergence per the API compatibility requirements. This was subsequently reverted in commit 5953a6a7.

## Benchmarks

| Workload | API | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|----------|-----|--------------|----------------|---------|
| Kubernetes (50 KB) | scan | 70.7 | 40.1 | 1.76x |
| Kubernetes (50 KB) | parse | 112.7 | 31.1 | 3.62x |
| Kubernetes (50 KB) | load | 73.2 | 28.5 | 2.57x |
| Mapping (244 KB) | scan | 133.2 | 36.8 | 3.62x |
| Mapping (244 KB) | parse | 138.3 | 55.5 | 2.49x |
| Mapping (244 KB) | load | 198.3 | 27.5 | 7.21x |
| Flow (57 KB) | scan | 79.4 | 17.7 | 4.49x |
| Flow (57 KB) | parse | 66.1 | 19.1 | 3.46x |
| Flow (57 KB) | load | 81.9 | 13.3 | 6.16x |
| Small (35 B) | scan | 28.4 | 14.0 | 2.03x |
| Small (35 B) | parse | 20.1 | 9.7 | 2.07x |
| Small (35 B) | load | 16.0 | 10.4 | 1.54x |

**Note:** Benchmark numbers appear lower than previous reports. This is expected — this run was on a shared system with other concurrent workloads (ASAN tests, fuzz campaigns running in parallel). Relative ratios are more meaningful than absolute numbers.

## Summary

- **Unit tests:** PASS (40/41, 1 excluded)
- **ASAN+UBSAN:** PASS (39/39 clean)
- **Differential:** FAIL — 1 error message divergence (tab error message)
- **Fuzz:** FAIL — 1 crash from differential divergence
- **Benchmarks:** 1.54x - 7.21x vs libyaml

**Overall: FAIL** — Divergence in scanner error message for tab characters. This was fixed in the subsequent commit 5953a6a7 which reverted the tab message change.

## Total Validation Time

~120s (build + unit tests + ASAN + differential + fuzz + benchmarks)
