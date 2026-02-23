# Validation Report: 53d39972

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** 53d39972 - perf: Force-inline loader_create_plain_scalar to recover mapping throughput
**Also covers:** e89b86ff - ops: Gitignore fuzz corpus directories
**Total validation time:** ~2 minutes

## Stage 1: Build

- Release build: OK (gcc, -O2)
- ASAN+UBSAN build: OK (clang, Debug, -fsanitize=address,undefined)

## Stage 2: Unit Tests (Release)

**37/37 tests pass** (0.57 seconds)

## Stage 3: Differential Tests

- Internal differential: **172/172 passed**, 0 divergences
- Differential vs libyaml (dlopen): **93/93 passed**, 0 divergences

**No divergences from libyaml.**

## Stage 4: ASAN+UBSAN

**36/37 pass** (9.70 seconds)

- 1 expected failure: `conformance-test-reader` (known test harness leak)
- **Zero ASAN errors** (no heap-buffer-overflow, use-after-free, etc.)
- **Zero UBSAN errors** (no undefined behavior)
- truncation, oom, coverage-targets all pass clean (sequence batch leaks fixed in earlier commits)

## Stage 5: Quick Fuzz (ASAN-enabled, 15s per harness)

| Harness | Runs | Exec/sec | New Units | Crashes |
|---------|------|----------|-----------|---------|
| fuzz_scan | 300,380 | 18,773 | 455 | 0 |
| fuzz_parse | 1,000,658 | 62,541 | 1,527 | 0 |
| fuzz_load | 675,096 | 42,193 | 717 | 0 |
| fuzz_structured | 1,188,616 | 74,288 | 999 | 0 |
| **Total** | **3,164,750** | | **3,698** | **0** |

## Stage 6: Benchmarks (Release -O2, 2 runs averaged)

### qyaml (commit 53d39972)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 449 | 399 | 375 |
| Mapping (244KB) | 746 | 652 | 713 |
| Flow (57KB) | 321 | 284 | 261 |
| Small (35B) | 107 | 76 | 55 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 144 | 128 | 87 |
| Mapping (244KB) | 151 | 138 | 104 |
| Flow (57KB) | 110 | 95 | 61 |
| Small (35B) | 69 | 49 | 31 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.12x | 3.12x | 4.31x |
| Mapping | 4.94x | 4.72x | 6.86x |
| Flow | 2.92x | 2.99x | 4.28x |
| Small | 1.55x | 1.55x | 1.77x |

### Load throughput history (mapping workload)

| Commit | Load (MB/s) | vs libyaml | Note |
|--------|-------------|------------|------|
| bd4fb748 (refactor) | 659 | 6.05x | Regression from helper extraction |
| c478159b (seq batch) | 762 | 6.93x | +15.6% (seq batch + inlining) |
| **53d39972 (force-inline)** | **713** | **6.86x** | **+8.2% vs bd4fb7, -6.4% vs c47815** |

The force-inline recovers most but not all of the mapping regression. Mapping load went from 659 -> 713 MB/s (+8.2%). The remaining gap vs c478159b (762 MB/s) is likely because c478159b included the sequence batch which also improved mapping throughput via code layout effects.

## Verdict: PASS

All stages clean. ASAN back to baseline (36/37). Zero fuzz crashes across 3.2M runs. Zero differential divergences. Force-inline successfully recovers most of the mapping load regression.
