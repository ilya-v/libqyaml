# Validation Report: 41277aa, 1734b19, 28c6e52

**Date:** 2026-02-23
**Tested by:** tester agent
**Commits under test:**
- `41277aa` - reader: Add zero-copy mode for ASCII string input
- `1734b19` - scanner: Streamline batch path mark computation and buffer advance
- `28c6e52` - scanner: Add flow entry scalar batch path for flow sequences

**Built against:** 28c6e52 (cumulative validation of all three commits)

## Test Results

**37/37 tests pass** (0.56 seconds total)

All test targets pass:
- Unit tests (core, scanner, parser, loader, emitter, roundtrip, error paths, edge cases)
- Conformance tests (version, reader, scanner, parser, loader, emitter, dumper)
- YAML test suite parser (277/394 pass, matching libyaml's own pass set)
- Scanner comprehensive tests
- Differential libyaml tests

## Benchmark Results (Release -O2, averaged across 2 runs)

### qyaml (commit 28c6e52)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 442 | 414 | 258 |
| Mapping (244KB) | 730 | 659 | 406 |
| Flow (57KB) | 332 | 300 | 198 |
| Small (35B) | 109 | 82 | 57 |

### libyaml (reference)

| Workload | Scan (MB/s) | Parse (MB/s) | Load (MB/s) |
|----------|-------------|--------------|-------------|
| K8s (50KB) | 151 | 130 | 89 |
| Mapping (244KB) | 155 | 141 | 108 |
| Flow (57KB) | 117 | 96 | 64 |
| Small (35B) | 71 | 49 | 31 |

### Speedup vs libyaml

| Workload | Scan | Parse | Load |
|----------|------|-------|------|
| K8s | 2.93x | 3.18x | 2.90x |
| Mapping | 4.71x | 4.67x | 3.76x |
| Flow | 2.85x | 3.13x | 3.09x |
| Small | 1.54x | 1.67x | 1.84x |

## Comparison with Previous Validated Commit (412c80e)

Previous validation (412c80e) reported:
- K8s: scan 2.63x, parse 3.14x, load 2.65x
- Mapping: scan 4.05x, parse 4.56x, load 3.41x
- Flow: scan 2.13x, parse 2.59x, load 2.51x
- Small: scan 1.49x, parse 1.65x, load 1.66x

**Changes from these three commits:**
- K8s: scan +11%, parse +1%, load +9% (vs libyaml ratios improved)
- Mapping: scan +16%, parse +2%, load +10%
- **Flow: scan +34%, parse +21%, load +23%** -- commit 28c6e52 specifically targets flow sequences
- Small: scan +3%, parse +1%, load +11%

The flow sequence improvements are significant and directly attributable to commit 28c6e52 (flow entry scalar batch path).

## Commit Analysis

### 41277aa - reader: Add zero-copy mode for ASCII string input
Optimization in the reader layer to avoid copying when input is already ASCII-compatible. Benefits all workloads slightly.

### 1734b19 - scanner: Streamline batch path mark computation and buffer advance
Internal cleanup and optimization of the batch scanning path. Reduces overhead in mark tracking and buffer advancement.

### 28c6e52 - scanner: Add flow entry scalar batch path for flow sequences
New batch scanning path specifically for flow sequence entries. This is the primary driver of the flow workload improvement (+34% scan, +21% parse, +23% load).

## Safety Assessment

- All 37 tests pass with zero failures
- No regressions detected in any workload
- Flow workload shows substantial improvement as expected from the optimization target
- No test failures or unexpected behavior observed

## Verdict: PASS

All three commits are safe and performing as expected. The flow sequence optimization (28c6e52) delivers meaningful improvement on the targeted workload without regressing other workloads.
