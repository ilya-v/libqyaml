# Validation Report: c478159b

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** c478159b - perf: Add batch loading fast path for block sequences

## Test Results

**37/37 tests pass** (0.48 seconds total)

All test targets pass: unit tests, conformance tests, YAML test suite parser (277/394), scanner comprehensive, differential libyaml tests, guard-page allocator, allocation tracker, deep nesting, truncation, resource constrained, stress, OOM, edge cases, roundtrip, emitter comprehensive, reader comprehensive, loader comprehensive, errors comprehensive, parser comprehensive, document API, event API, scalars.

## Benchmark Results (Release -O2, averaged across 2 runs)

### qyaml (commit c478159b)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 461 | 402 | 394 |
| Mapping (244KB) | 757 | 662 | 745 |
| Flow (57KB) | 322 | 284 | 263 |
| Small (35B) | 104 | 78 | 57 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 151 | 131 | 89 |
| Mapping (244KB) | 151 | 140 | 112 |
| Flow (57KB) | 114 | 100 | 67 |
| Small (35B) | 70 | 49 | 31 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.05x | 3.07x | 4.43x |
| Mapping | 5.01x | 4.73x | 6.65x |
| Flow | 2.82x | 2.84x | 3.93x |
| Small | 1.49x | 1.59x | 1.84x |

## Comparison with Previous Validated Commit (76ee064a)

Previous (76ee06) load numbers: K8s 398, Mapping 758, Flow 263, Small 52

**Changes from sequence batch path (c478159b):**
- K8s load: 398 -> 394 MB/s (within noise, -1%)
- Mapping load: 758 -> 745 MB/s (within noise, -2%)
- Flow load: 263 -> 263 MB/s (unchanged)
- Small load: 52 -> 57 MB/s (+10%)

The sequence batch path shows no measurable improvement on K8s or flow workloads in these benchmarks. The K8s workload contains sequences but the batch path may not be triggered for the specific sequence patterns in use. Small document load improved slightly (+10%), possibly from code layout changes. Mapping shows slight noise-level variation.

Note: Scan and parse numbers are expected to be unaffected (loader-only change) and remain consistent.

## Verdict: PASS

All 37 tests pass. No regressions detected. Performance is stable relative to 76ee064a. The sequence batch path does not produce measurable throughput gains on the current benchmark workloads but introduces no regressions either. The worker may want to investigate whether the batch path is actually triggered by the K8s workload.
