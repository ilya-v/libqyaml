# Validation Report: 07b61054

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** 07b61054 - fix: Eliminate heap-use-after-free in batch mapping loader
**Total validation time:** ~2 minutes

## Stage 1: Build

- Release build: OK (gcc, -O2)
- ASAN+UBSAN build: OK (clang, Debug, -fsanitize=address,undefined)

## Stage 2: Unit Tests (Release)

**37/37 tests pass** (0.53 seconds)

All test targets pass: version, reader, scanner, parser, event-api, document-api, emitter, scalars, edge-cases, differential, errors, writer, roundtrip, stress, truncation, deep-nesting, oom, coverage-targets, guard-page, alloc-tracker, api-comprehensive, reader-comprehensive, scanner-comprehensive, parser-comprehensive, emitter-comprehensive, errors-comprehensive, loader-comprehensive, conformance tests, yaml-test-suite (277/394).

## Stage 3: ASAN+UBSAN

**36/37 pass** (9.95 seconds)

- 1 expected failure: `conformance-test-reader` (known test harness leak, not a library bug)
- **Zero ASAN errors** (no heap-buffer-overflow, use-after-free, etc.)
- **Zero UBSAN errors** (no undefined behavior)
- Truncation, OOM, and coverage-targets all pass under ASAN

## Stage 4: Quick Fuzz (ASAN-enabled, 20s per harness)

| Harness | Runs | Exec/sec | New Units | Crashes |
|---------|------|----------|-----------|---------|
| fuzz_scan | 823,262 | 39,202 | 1,290 | 0 |
| fuzz_parse | 967,208 | 46,057 | 2,159 | 0 |
| fuzz_load | 837,501 | 39,881 | 2,148 | 0 |
| fuzz_structured | 1,203,813 | 57,324 | 609 | 0 |
| **Total** | **3,831,784** | | **6,206** | **0** |

**UAF crash reproducers verified fixed:**
- `crash-a60aea3855295b5f633b2205e43db1d30c2edb6c` -- no crash
- `crash-a1ffd4d9a6669be4e29dd0c62bd4ab12675af6be` -- no crash
- `leak-3fa0e5054cc5e21e08893164b47fdbf7e9d90ed7` -- no leak

## Stage 5: Benchmarks (Release -O2)

### qyaml (commit 07b61054)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 453 | 388 | 385 |
| Mapping (244KB) | 748 | 653 | 722 |
| Flow (57KB) | 329 | 289 | 261 |
| Small (35B) | 106 | 79 | 57 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 142 | 127 | 88 |
| Mapping (244KB) | 148 | 139 | 106 |
| Flow (57KB) | 110 | 96 | 63 |
| Small (35B) | 68 | 48 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.19x | 3.06x | 4.38x |
| Mapping | 5.05x | 4.70x | 6.81x |
| Flow | 2.99x | 3.01x | 4.14x |
| Small | 1.56x | 1.65x | 1.78x |

No performance regressions from the UAF fix. All numbers within noise of previous measurements.

## Verdict: PASS

All stages clean. The critical heap-use-after-free bug is confirmed fixed. All 3 crash/leak reproducers pass clean. Zero new ASAN errors. Zero fuzz crashes across 3.8M runs. No performance regressions.
