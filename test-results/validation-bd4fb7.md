# Validation Report: bd4fb748

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** bd4fb748 - refactor: extract loader_create_plain_scalar helper, fix OOM leak
**Total validation time:** ~2 minutes

## Stage 1: Build

- Release build: OK (gcc, -O2)
- ASAN+UBSAN build: OK (clang, Debug, -fsanitize=address,undefined)

## Stage 2: Unit Tests (Release)

**37/37 tests pass** (0.49 seconds)

All test targets pass: version, reader, scanner, parser, event-api, document-api, emitter, scalars, edge-cases, differential, errors, writer, roundtrip, stress, truncation, deep-nesting, oom, coverage-targets, guard-page, alloc-tracker, api-comprehensive, reader-comprehensive, scanner-comprehensive, parser-comprehensive, emitter-comprehensive, errors-comprehensive, loader-comprehensive, conformance tests, yaml-test-suite (277/394).

## Stage 3: Differential Tests

- Internal differential: **172/172 passed**, 0 divergences
- Differential vs libyaml (dlopen): **93/93 passed**, 0 divergences
- API levels tested: parser events (full), scanner tokens (partial), loaded documents (partial)
- Inputs: embedded YAML test cases, benchmark workload subsets

**No divergences from libyaml.**

## Stage 4: ASAN+UBSAN

**36/37 pass** (9.70 seconds)

- 1 expected failure: `conformance-test-reader` (known test harness leak, not a library bug)
- **Zero ASAN errors** (no heap-buffer-overflow, use-after-free, etc.)
- **Zero UBSAN errors** (no undefined behavior)
- OOM, truncation, and coverage-targets all pass under ASAN

## Stage 5: Quick Fuzz (ASAN-enabled, 20s per harness)

| Harness | Runs | Exec/sec | New Units | Crashes |
|---------|------|----------|-----------|---------|
| fuzz_scan | 1,714,522 | 81,643 | 8,247 | 0 |
| fuzz_parse | 1,852,044 | 88,192 | 8,341 | 0 |
| fuzz_load | 1,586,820 | 75,562 | 8,210 | 0 |
| fuzz_structured | 1,650,879 | 78,613 | 3,455 | 0 |
| **Total** | **6,804,265** | | **28,253** | **0** |

## Stage 6: Benchmarks (Release -O2, 2 runs averaged)

### qyaml (commit bd4fb748)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 441 | 408 | 366 |
| Mapping (244KB) | 726 | 650 | 659 |
| Flow (57KB) | 316 | 279 | 269 |
| Small (35B) | 107 | 76 | 55 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 145 | 126 | 86 |
| Mapping (244KB) | 151 | 140 | 109 |
| Flow (57KB) | 114 | 99 | 66 |
| Small (35B) | 66 | 49 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.04x | 3.24x | 4.26x |
| Mapping | 4.81x | 4.64x | 6.05x |
| Flow | 2.77x | 2.82x | 4.08x |
| Small | 1.62x | 1.55x | 1.72x |

### Comparison with previous commit (07b61054)

| Workload | 07b610 Load | bd4fb7 Load | Delta |
|----------|-------------|-------------|-------|
| K8s | 385 MB/s | 366 MB/s | -4.9% |
| Mapping | 722 MB/s | 659 MB/s | -8.7% |
| Flow | 261 MB/s | 269 MB/s | +3.1% |
| Small | 57 MB/s | 55 MB/s | -3.5% |

**Note:** Mapping and K8s load show a small regression (5-9%). This may be within noise or a side-effect of the refactor changing code layout / inlining. The refactor was primarily a cleanup (-53 lines, deduplication) and OOM leak fix. Worker should verify if the regression is real and acceptable.

## Verdict: PASS (with regression note)

All stages clean. Zero ASAN/UBSAN errors. Zero fuzz crashes across 6.8M runs. Zero differential divergences. The OOM leak fix is confirmed working (OOM test passes under ASAN). Small load throughput regression on mapping/K8s workloads warrants attention from the worker.
