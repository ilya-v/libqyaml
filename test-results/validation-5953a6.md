# Validation Report: 5953a6a7

**Commit:** 5953a6a7 — fix: Revert tab error message change that diverged from libyaml
**Date:** 2026-02-23
**Tested by:** tester-3

## Build

| Build | Status |
|-------|--------|
| Release (-O2) | PASS |
| ASAN+UBSAN (clang) | PASS |

## Unit Tests (Release)

- **Result:** 40/40 passed, 0 failed (coverage-targets excluded — known hang)
- **Total time:** 0.62s

## Differential Tests

- **test-differential-libyaml:** 294/294 passed — zero divergences on curated inputs
- **Differential fuzzer:** DIVERGENCE FOUND (see below)

## ASAN+UBSAN

- **Result:** 39/39 passed, 0 errors
- All tests clean under AddressSanitizer and UndefinedBehaviorSanitizer
- Total time: 11.0s

## Quick Fuzz

| Harness | Executions | Crashes | Divergences | Time |
|---------|-----------|---------|-------------|------|
| fuzz_scan | 354,132 | 0 | 0 | 7s |
| fuzz_parse | 418,224 | 0 | 0 | 7s |
| fuzz_load | 348,208 | 0 | 0 | 7s |
| fuzz_differential | 16 | 1 | 1 | <1s |
| **Total** | **1,120,580** | **1** | **1** | **21s** |

### CRITICAL: Scanner Status Divergence

**Input:** `foo:\n  bar\ninvalid\n` (19 bytes)
**Level:** Scanner — status divergence (ours=0/failure, ref=1/success)

**Our token stream:**
```
STREAM_START -> BLOCK_MAP_START -> KEY -> SCALAR("foo") -> VALUE -> ERROR("could not find expected ':'")
```

**Reference libyaml token stream:**
```
STREAM_START -> BLOCK_MAP_START -> KEY -> SCALAR("foo") -> VALUE -> SCALAR("bar") -> ERROR("could not find expected ':'")
```

Our scanner fails to emit the SCALAR("bar") token that libyaml successfully produces before both reach the same error. The error occurs because "invalid" at column 0 breaks the mapping, but libyaml correctly scans the value "bar" first before encountering the error. Our scanner appears to hit the error prematurely, skipping the value scalar.

This is a pre-existing bug, not introduced by this commit. The revert commit (5953a6a7) successfully fixed the tab error message divergence from 07faa3aa, but this separate scanner divergence was already present.

**Reproducer saved to:** `test-output/fuzz-crashes-percommit/fuzz_differential/crash-aeeaefdddf1ab3980fc0b4c37f2ecffb7d8033c4`

## Benchmarks

| Workload | API | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|----------|-----|--------------|----------------|---------|
| Kubernetes (50 KB) | scan | 350.6 | 125.2 | 2.80x |
| Kubernetes (50 KB) | parse | 370.1 | 113.0 | 3.27x |
| Kubernetes (50 KB) | load | 367.5 | 78.2 | 4.70x |
| Mapping (244 KB) | scan | 698.4 | 133.4 | 5.24x |
| Mapping (244 KB) | parse | 601.5 | 121.3 | 4.96x |
| Mapping (244 KB) | load | 707.0 | 91.3 | 7.74x |
| Flow (57 KB) | scan | 239.2 | 93.8 | 2.55x |
| Flow (57 KB) | parse | 246.1 | 80.2 | 3.07x |
| Flow (57 KB) | load | 256.3 | 58.2 | 4.41x |
| Small (35 B) | scan | 91.4 | 62.6 | 1.46x |
| Small (35 B) | parse | 73.5 | 42.3 | 1.74x |
| Small (35 B) | load | 53.0 | 27.2 | 1.95x |

**Performance range:** 1.46x - 7.74x vs libyaml
**Peak:** Mapping load at 707.0 MB/s (7.74x)

## Summary

- **Unit tests:** PASS (40/40)
- **ASAN+UBSAN:** PASS (39/39 clean)
- **Differential (curated):** PASS (294/294)
- **Differential (fuzz):** FAIL — 1 scanner status divergence (missing SCALAR token before error)
- **Benchmarks:** 1.46x - 7.74x vs libyaml

**Overall: FAIL** — Scanner divergence: our library fails to emit a value scalar that libyaml successfully produces before an error condition. This is a pre-existing bug (not introduced by this commit). Worker has been notified.

## Total Validation Time

~120s (build + unit tests + ASAN + differential + fuzz + benchmarks)
