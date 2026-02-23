# Validation Report: c478159b

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** c478159b - perf: Add batch loading fast path for block sequences
**Total validation time:** ~2 minutes

## Stage 1: Build

- Release build: OK (gcc, -O2)
- ASAN+UBSAN build: OK (clang, Debug, -fsanitize=address,undefined)

## Stage 2: Unit Tests (Release)

**37/37 tests pass** (0.48 seconds)

All test targets pass.

## Stage 3: Differential Tests

- Internal differential: **172/172 passed**, 0 divergences
- Differential vs libyaml (dlopen): **93/93 passed**, 0 divergences

**No divergences from libyaml.**

## Stage 4: ASAN+UBSAN

**33/37 pass** (9.75 seconds) -- **REGRESSION**

- 1 expected failure: `conformance-test-reader` (known test harness leak)
- **3 NEW failures (memory leaks):**
  - `truncation`: 463 bytes leaked in 88 allocations
  - `oom`: ~147 bytes leaked in ~22 allocations
  - `coverage-targets`: 4 bytes leaked in 1 allocation

**Root cause:** The new `yaml_parser_load_sequence_items_batch()` function has error-path leaks identical to the pattern previously fixed in the mapping batch function. When the batch function does `return 0` (on OOM at line 1004, 1005, or 1011), the scanner has already allocated scalar values in the token queue. The batch function consumed the BLOCK_ENTRY token (line 994) but the scalar token's value is not properly freed on error exit.

**Confirmed regression:** These 3 tests all pass on `bd4fb748` (previous commit) and fail on `c478159b`.

**This is a known bug pattern** -- the exact same leak was found and fixed in the mapping batch function (commit 481524f2). The fix approach should be the same: ensure all error-path `return 0` statements properly handle cleanup of scanner-allocated scalar values.

## Stage 5: Quick Fuzz (ASAN-enabled, 15s per harness)

| Harness | Runs | Exec/sec | New Units | Crashes |
|---------|------|----------|-----------|---------|
| fuzz_scan | 528,177 | 33,011 | 1,235 | 0 |
| fuzz_parse | 1,068,298 | 66,768 | 2,192 | 0 |
| fuzz_load | 693,484 | 43,342 | 1,025 | 0 |
| fuzz_structured | 1,211,604 | 75,725 | 1,183 | 0 |
| **Total** | **3,501,563** | | **5,635** | **0** |

No crashes. No ASAN violations in fuzzing (leaks are not detected in fuzz mode).

## Stage 6: Benchmarks (Release -O2)

### qyaml (commit c478159b)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 436 | 382 | 397 |
| Mapping (244KB) | 749 | 661 | 762 |
| Flow (57KB) | 326 | 297 | 266 |
| Small (35B) | 91 | 75 | 53 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 146 | 132 | 89 |
| Mapping (244KB) | 153 | 141 | 110 |
| Flow (57KB) | 115 | 100 | 66 |
| Small (35B) | 69 | 50 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 2.99x | 2.89x | 4.46x |
| Mapping | 4.90x | 4.69x | 6.93x |
| Flow | 2.84x | 2.97x | 4.03x |
| Small | 1.32x | 1.50x | 1.66x |

### Load comparison vs previous commit (bd4fb748)

| Workload | bd4fb7 Load | c47815 Load | Delta |
|----------|-------------|-------------|-------|
| K8s | 366 MB/s | 397 MB/s | **+8.5%** |
| Mapping | 659 MB/s | 762 MB/s | **+15.6%** |
| Flow | 269 MB/s | 266 MB/s | -1.1% |
| Small | 55 MB/s | 53 MB/s | -3.6% |

The sequence batch path provides a significant **+8.5% improvement on K8s** and **+15.6% on mapping** load. K8s improvement makes sense -- K8s YAML contains many sequences of plain scalars. Mapping improvement is likely from reduced overhead of the code restructuring.

## Verdict: FAIL (ASAN memory leaks)

The sequence batch optimization shows strong performance improvements on K8s (+8.5%) and mapping (+15.6%) workloads. However, it introduces 3 new ASAN leak failures on error paths. These are the same class of bug as the mapping batch leak (commit 481524f2). The fix pattern is known -- handle cleanup of scanner-allocated scalar values on all error-path returns.
