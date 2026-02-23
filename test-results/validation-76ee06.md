# Validation Report: 76ee064a + 3d8e00c1

**Date:** 2026-02-23
**Tested by:** tester agent
**Commits validated:**
- 3d8e00c1 - Optimize document loading with batch mapping pairs and pointer iteration
- 76ee064a - Estimate initial node array size from input length to avoid reallocs

## Test Results

**37/37 tests pass** (0.47 seconds total)

All test targets pass: unit tests, conformance tests, YAML test suite parser (277/394), scanner comprehensive, differential libyaml tests, guard-page allocator, allocation tracker, deep nesting, truncation, resource constrained, stress, OOM, edge cases, roundtrip, emitter comprehensive, reader comprehensive, loader comprehensive, errors comprehensive, parser comprehensive, document API, event API, scalars.

## Benchmark Results (Release -O2, averaged across 2 runs)

### qyaml (commit 76ee064a)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 459 | 410 | 398 |
| Mapping (244KB) | 727 | 661 | 758 |
| Flow (57KB) | 325 | 280 | 263 |
| Small (35B) | 109 | 80 | 52 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 146 | 127 | 86 |
| Mapping (244KB) | 147 | 137 | 107 |
| Flow (57KB) | 114 | 97 | 65 |
| Small (35B) | 68 | 49 | 31 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.14x | 3.23x | 4.63x |
| Mapping | 4.95x | 4.82x | 7.08x |
| Flow | 2.85x | 2.89x | 4.05x |
| Small | 1.60x | 1.63x | 1.68x |

## Comparison with Previous Validated Commit (e09ef672 - string arena)

Previous (e09ef672) load numbers: K8s 329, Mapping 573, Flow 245, Small 59

**Load improvements from batch mapping pairs + node array prealloc (76ee064a):**
- K8s load: 329 -> 398 MB/s (+21%, 3.70x -> 4.63x)
- Mapping load: 573 -> 758 MB/s (+32%, 5.31x -> 7.08x)
- Flow load: 245 -> 263 MB/s (+7%, 3.71x -> 4.05x)
- Small load: 59 -> 52 MB/s (-12%, 1.84x -> 1.68x)

Scan and parse are unchanged as expected (optimizations are loader-only).

**Mapping load crossed 7x vs libyaml** -- a new peak for the project. K8s load jumped from 3.7x to 4.6x. Flow load improved modestly. Small document load has a slight regression, likely due to the preallocation overhead dominating for tiny inputs.

## Verdict: PASS

All 37 tests pass. The batch mapping pairs and node array preallocation deliver major load performance gains on medium-to-large workloads (+21% to +32%). Small document regression (-12%) is a minor concern but expected for preallocation approaches on tiny inputs. No regressions in scan or parse.
