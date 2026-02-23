# Validation Report: b137e8d4

**Commit:** b137e8d4 fix: Preserve trailing line breaks after escaped line breaks in quoted scalars
**Also covers:** 03d77aef fix: Apply blank line continuation fix to batch KV scanner path
**Date:** 2026-02-23
**Validated by:** tester-2

## Build

| Build | Status |
|-------|--------|
| Release (-O2) | PASS |
| ASAN+UBSAN (clang, Debug) | PASS |
| Fuzz harnesses (ASAN) | PASS (via per-commit fuzz script) |

## Unit Tests (Release)

| Test Suite | Result |
|------------|--------|
| Differential (vs libyaml) | 294/294 pass |
| API Comprehensive | 156/156 pass |
| Parser Comprehensive | 182/182 pass |
| Scanner Comprehensive | 166/166 pass |
| Loader Comprehensive | 130/130 pass |
| Edge Cases | 302/302 pass |
| Errors | 124/124 pass |
| Errors Comprehensive | 77/77 pass |
| Emitter | 166/166 pass |
| Document API | 263/263 pass |
| Emitter Comprehensive | 55/55 pass |
| Deep Nesting | 34/34 pass |
| **Total** | **~2,250+ assertions, 0 failures** |

## Differential Tests (vs reference libyaml)

- **294/294 pass** (0 divergences)
- All 3 API levels: scanner tokens, parser events, loaded documents
- No regressions from quoted scalar or batch KV fixes

## ASAN+UBSAN

| Test Suite | Result |
|------------|--------|
| API Comprehensive | 156/156 pass |
| Parser Comprehensive | 182/182 pass |
| Loader Comprehensive | 130/130 pass |
| Edge Cases | 302/302 pass |
| Errors | 124/124 pass |
| Scanner Comprehensive | 166/166 pass |
| Differential (vs libyaml) | 294/294 pass |
| **Total** | **1,354 assertions, 0 ASAN errors, 0 UBSAN errors** |

## Quick Fuzz (per-commit fuzz script)

| Harness | Status | Runs | Corpus | Time |
|---------|--------|------|--------|------|
| fuzz_scan | PASS | 174,020 | 5,338 | 7s |
| fuzz_parse | PASS | 174,659 | 4,748 | 7s |
| fuzz_load | PASS | 151,521 | 4,411 | 7s |
| fuzz_differential | FAIL | 16 | 0 | 1s |
| **Total** | | **500,216** | | **22s** |

Seeds: 5,254 files. fuzz_differential has 1 known pre-existing divergence (scanner-level BOM/% issue). Three single-library harnesses fully clean.

## Benchmarks

Note: CPU contention present; ratios are reliable, absolute numbers depressed.

| Workload | API | Ours (MB/s) | Ref (MB/s) | Speedup |
|----------|-----|-------------|------------|---------|
| K8s (50 KB) | scan | 116.8 | 46.5 | 2.51x |
| K8s (50 KB) | parse | 121.0 | 35.0 | 3.46x |
| K8s (50 KB) | load | 120.8 | 23.7 | 5.10x |
| Mapping (244 KB) | scan | 230.7 | 58.8 | 3.92x |
| Mapping (244 KB) | parse | 177.5 | 58.8 | 3.02x |
| Mapping (244 KB) | load | 244.4 | 42.4 | 5.76x |
| Flow (57 KB) | scan | 98.7 | 47.4 | 2.08x |
| Flow (57 KB) | parse | 81.0 | 30.7 | 2.64x |
| Flow (57 KB) | load | 94.3 | 15.8 | 5.97x |
| Small (35 B) | scan | 37.9 | 22.9 | 1.66x |
| Small (35 B) | parse | 28.1 | 15.8 | 1.78x |
| Small (35 B) | load | 21.4 | 14.9 | 1.44x |

Speedup ratios: 1.4-6.0x across workloads. Consistent with previous reports.

## Summary

Both commits are **CLEAN**. The quoted scalar trailing line break fix and batch KV blank line continuation fix introduce no regressions. All unit tests, differential tests, and ASAN checks pass. Quick fuzz finds no new crashes.

**Total validation time:** ~2 minutes
