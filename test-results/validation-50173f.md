# Validation Report: 50173f41

**Date:** 2026-02-23
**Tested by:** tester-3
**Commit:** 50173f41 - perf: Add flow sequence batch loader bypassing parser for plain scalars
**Total validation time:** ~2 minutes

## Stage 1: Build

- Release build: OK (gcc, -O2)
- ASAN+UBSAN build: OK (clang, Debug, -fsanitize=address,undefined)

## Stage 2: Unit Tests (Release)

**37/37 tests pass** (0.52 seconds)

All test targets pass including conformance tests and yaml-test-suite (277/394).

## Stage 3: Differential Tests

- Differential vs libyaml (dlopen): **279/279 passed**, 0 divergences
- API levels tested: scanner tokens, parser events, loaded documents
- Inputs: 93 YAML test cases covering all input types

**No divergences from libyaml at any API level.**

## Stage 4: ASAN+UBSAN

**37/37 pass** (10.04 seconds)

- **Zero ASAN errors** (no heap-buffer-overflow, use-after-free, etc.)
- **Zero UBSAN errors** (no undefined behavior)
- **Zero leaks** (conformance-test-reader leak fixed in 0bd6153f)

## Stage 5: Quick Fuzz (ASAN-enabled, 15s per harness)

| Harness | Runs | Exec/sec | New Units | Crashes |
|---------|------|----------|-----------|---------|
| fuzz_scan | 1,009,830 | 63,114 | 0 | 0 |
| fuzz_parse | 1,426,423 | 89,151 | 2 | 0 |
| fuzz_load | 243,523 | 15,220 | 2 | 0 |
| fuzz_structured | 1,200,715 | 75,044 | 1 | 0 |
| **Total** | **3,880,491** | | **5** | **0** |

## Stage 6: Benchmarks (Release -O2, 2 runs averaged)

### qyaml (commit 50173f41)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 459 | 410 | 389 |
| Mapping (244KB) | 754 | 662 | 740 |
| Flow (57KB) | 324 | 288 | 335 |
| Small (35B) | 110 | 80 | 58 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 150 | 132 | 90 |
| Mapping (244KB) | 155 | 142 | 111 |
| Flow (57KB) | 114 | 98 | 66 |
| Small (35B) | 69 | 49 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.06x | 3.11x | 4.32x |
| Mapping | 4.86x | 4.66x | 6.67x |
| Flow | 2.84x | 2.94x | **5.08x** |
| Small | 1.59x | 1.63x | 1.81x |

### Performance Delta vs Previous Commit (53d39972)

| Workload | 53d399 Load | 50173f Load | Delta |
|----------|-------------|-------------|-------|
| K8s | 383 MB/s | 389 MB/s | +1.6% |
| Mapping | 709 MB/s | 740 MB/s | **+4.4%** |
| Flow | 271 MB/s | 335 MB/s | **+23.6%** |
| Small | 58 MB/s | 58 MB/s | 0% |

**The flow sequence batch loader delivers a major improvement.** Flow load jumped from 271 to 335 MB/s (+23.6%), and the speedup vs libyaml went from 4.11x to 5.08x -- breaking the 5x barrier on flow load for the first time. Mapping load also improved to 740 MB/s (+4.4%), likely from code layout effects. Scan and parse are unchanged as expected (the optimization is load-only).

## Verdict: PASS

All stages clean. 37/37 ASAN+UBSAN (zero exceptions). Zero fuzz crashes across 3.9M runs. 279/279 differential pass at all 3 API levels. Flow load throughput improved 24% with the batch loader optimization. No regressions on any workload.
