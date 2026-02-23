# Validation Report: e09ef672

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** e09ef672 - loader: Add string arena for bulk scalar value allocation

## Test Results

**37/37 tests pass** (0.55 seconds total)

All test targets pass: unit tests, conformance tests, YAML test suite parser (277/394), scanner comprehensive, differential libyaml tests.

## Benchmark Results (Release -O2, averaged across 2 runs)

### qyaml (commit e09ef672)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 471 | 410 | 329 |
| Mapping (244KB) | 764 | 653 | 573 |
| Flow (57KB) | 324 | 295 | 245 |
| Small (35B) | 110 | 85 | 59 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 150 | 131 | 89 |
| Mapping (244KB) | 154 | 135 | 108 |
| Flow (57KB) | 110 | 99 | 66 |
| Small (35B) | 71 | 49 | 32 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 3.14x | 3.13x | 3.70x |
| Mapping | 4.96x | 4.84x | 5.31x |
| Flow | 2.95x | 2.98x | 3.71x |
| Small | 1.55x | 1.73x | 1.84x |

## Comparison with Previous Validated Commit (28c6e52)

Previous (28c6e52) load numbers: K8s 258, Mapping 406, Flow 198, Small 57

**Load improvements from string arena (e09ef672):**
- K8s load: 258 -> 329 MB/s (+28%, 2.90x -> 3.70x)
- Mapping load: 406 -> 573 MB/s (+41%, 3.76x -> 5.31x)
- Flow load: 198 -> 245 MB/s (+24%, 3.09x -> 3.71x)
- Small load: 57 -> 59 MB/s (+4%, 1.84x -> 1.84x)

Scan and parse are unchanged as expected (arena only affects the loader path).

**This is the single largest load improvement this session.** Mapping load crossed 5x vs libyaml for the first time.

## Verdict: PASS

All tests pass. The string arena delivers substantial load performance gains (+24% to +41%) with no regressions in scan or parse. Memory safety will be verified in the next background ASAN/Valgrind pass.
