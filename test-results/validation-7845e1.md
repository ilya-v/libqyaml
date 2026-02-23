# Validation Report: 7845e16e + 995cd20d

**Date:** 2026-02-23
**Tested by:** tester agent
**Commits validated:**
- 995cd20d - fix: Prevent potential double-free in sequence batch error path
- 7845e16e - perf: Fix small document regression from node array preallocation

## Test Results

**37/37 tests pass** (0.50 seconds total)

## Benchmark Results (Release -O2, averaged across 2 runs)

### qyaml (commit 7845e16e)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 456 | 407 | 385 |
| Mapping (244KB) | 758 | 661 | 749 |
| Flow (57KB) | 325 | 291 | 267 |
| Small (35B) | 105 | 80 | 57 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 151 | 132 | 91 |
| Mapping (244KB) | 157 | 141 | 113 |
| Flow (57KB) | 114 | 99 | 67 |
| Small (35B) | 71 | 49 | 31 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.02x | 3.08x | 4.23x |
| Mapping | 4.83x | 4.69x | 6.63x |
| Flow | 2.85x | 2.94x | 3.99x |
| Small | 1.48x | 1.63x | 1.84x |

## Comparison with Previous (481524f2)

Previous small doc load: 57 MB/s (1.78x). Current: 57 MB/s (1.84x vs updated libyaml baseline). The small document regression from node array preallocation (was 52 MB/s at 76ee06) is now fully recovered back to pre-prealloc levels (59 MB/s at e09ef6).

Other workloads are within noise of previous measurements. No regressions.

## Use-After-Free Status

**Still open.** Crash reproducer (`crash-a60aea*`) still triggers heap-use-after-free at loader.c:938 in `yaml_parser_load_mapping_pairs_batch`. Neither the sequence batch double-free fix (995cd20d) nor the small doc regression fix (7845e16e) addressed the token queue pointer invalidation bug.

## Verdict: PASS (with known open bug)

All 37 tests pass. Small document regression fixed. No performance regressions. The heap-use-after-free in batch mapping remains the sole outstanding critical issue.
