# Validation Report: f909125

**Commit:** f909125 - Skip simple key scan in fetch_more_tokens when no keys possible
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.58 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs c341e2a |
|-----------|--------------|----------------|---------|------------|
| scan | 260.8 | 148.0 | 1.76x | -0.1% |
| parse | 254.0 | 131.1 | 1.94x | **+8.4%** |
| load | 175.9 | 88.2 | 1.99x | +4.5% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs c341e2a |
|-----------|--------------|----------------|---------|------------|
| scan | 360.5 | 154.3 | 2.34x | -1.0% |
| parse | 364.6 | 138.3 | 2.64x | **+5.5%** |
| load | 240.7 | 104.5 | 2.30x | +2.2% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs c341e2a |
|-----------|--------------|----------------|---------|------------|
| scan | 210.7 | 111.9 | 1.88x | +2.6% |
| parse | 195.6 | 96.7 | 2.02x | **+12.4%** |
| load | 129.4 | 60.1 | 2.15x | **+10.7%** |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs c341e2a |
|-----------|--------------|----------------|---------|------------|
| scan | 94.2 | 69.0 | 1.37x | +1.4% |
| parse | 75.2 | 47.2 | 1.59x | **+6.4%** |
| load | 52.3 | 30.6 | 1.71x | +4.8% |

## Performance Impact Summary

Skipping simple key scanning when no keys are possible is a major parse optimization:
- **K8s parse: +8.4%** (234.4 -> 254.0 MB/s), now 1.94x vs libyaml
- **Flow parse crosses 2x:** 174.0 -> 195.6 MB/s (+12.4%), now 2.02x vs libyaml
- **Flow load crosses 2x:** 116.9 -> 129.4 MB/s (+10.7%), now 2.15x vs libyaml
- **Mapping parse new peak:** 364.6 MB/s (2.64x vs libyaml -- best overall speedup)
- **Small doc parse:** +6.4% (70.7 -> 75.2 MB/s), now 1.59x
- Scan numbers essentially unchanged (expected -- optimization targets parser path)
- Overall speedup range vs libyaml: 1.37x - 2.64x
- Three operations now above 2x: mapping parse (2.64x), mapping load (2.30x), flow load (2.15x)
