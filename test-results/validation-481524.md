# Validation Report: 481524f2

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** 481524f2 - fix: Plug memory leaks in batch mapping error paths

## Test Results

**37/37 tests pass** (0.48 seconds total, release build)

ASAN+UBSAN: **36/37 pass** (up from 33/37 at 76ee06). Truncation, OOM, and coverage-targets tests now pass under ASAN. Only pre-existing conformance-test-reader failure remains. **Leak fix confirmed effective.**

## Benchmark Results (Release -O2, single run)

### qyaml (commit 481524f2)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 455 | 409 | 394 |
| Mapping (244KB) | 764 | 664 | 761 |
| Flow (57KB) | 330 | 299 | 272 |
| Small (35B) | 109 | 80 | 57 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 151 | 130 | 90 |
| Mapping (244KB) | 155 | 139 | 110 |
| Flow (57KB) | 117 | 99 | 65 |
| Small (35B) | 68 | 48 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.01x | 3.15x | 4.38x |
| Mapping | 4.93x | 4.78x | 6.92x |
| Flow | 2.82x | 3.02x | 4.18x |
| Small | 1.60x | 1.67x | 1.78x |

No performance regressions from the leak fix.

## Critical Bug Still Open: heap-use-after-free

**The use-after-free bug reported earlier is NOT fixed by this commit.** The crash reproducer (`test-output/fuzz/crashes/crash-a60aea3855295b5f633b2205e43db1d30c2edb6c`) still triggers heap-use-after-free at loader.c:937.

A fresh fuzz run found a second crash variant at loader.c:976 (crash-a1ffd4d9a6669be4e29dd0c62bd4ab12675af6be) in just 88K runs (~2 seconds).

Root cause remains: `key_scalar` and `value_scalar` are pointers into the token queue that become dangling when `yaml_queue_extend` reallocs the queue during a subsequent `LOADER_PEEK_TOKEN`. The leak fix added cleanup code but did not address the pointer invalidation.

## Verdict: PARTIAL PASS

Leak fix is effective (ASAN failures reduced from 4 to 1). No performance regressions. But the critical use-after-free remains unfixed. This library is unsafe for untrusted input until the dangling pointer issue is resolved.
