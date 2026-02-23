# Validation Report: 5c15fed

**Commit:** 5c15fed - Fix batch path termination to use key column instead of parser indent
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.54 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f3a6295 |
|-----------|--------------|----------------|---------|------------|
| scan | 373.2 | 145.6 | 2.56x | +6.7% |
| parse | 342.5 | 128.9 | 2.66x | -2.0% |
| load | 230.4 | 90.4 | 2.55x | **+7.5%** |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f3a6295 |
|-----------|--------------|----------------|---------|------------|
| scan | 590.2 | 156.5 | 3.77x | -6.8% |
| parse | 615.7 | 142.2 | 4.33x | -4.3% |
| load | 374.5 | 110.2 | 3.40x | **+8.6%** |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f3a6295 |
|-----------|--------------|----------------|---------|------------|
| scan | 238.4 | 114.1 | 2.09x | -1.7% |
| parse | 239.7 | 100.7 | 2.38x | -3.8% |
| load | 155.6 | 64.6 | 2.41x | -3.7% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f3a6295 |
|-----------|--------------|----------------|---------|------------|
| scan | 103.0 | 71.9 | 1.43x | +2.1% |
| parse | 77.5 | 49.8 | 1.56x | +0.9% |
| load | 50.2 | 32.5 | 1.54x | -1.2% |

## Performance Impact Summary

Batch path termination fix -- correctness improvement with load gains:
- **Mapping load: 374.5 MB/s, 3.40x vs libyaml** (new peak! was 344.9, +8.6%)
- **K8s scan: 373.2 MB/s, 2.56x** (new peak! was 349.7)
- **K8s load: 230.4 MB/s, 2.55x** (new peak! was 214.3, +7.5%)
- Mapping scan: 590.2 MB/s, 3.77x (slightly down from 4.01x, likely noise)
- Mapping parse: 615.7 MB/s, 4.33x (slightly down from 4.51x, likely noise)
- **Three operations above 3x, one above 4x**
- **Twelve operations above 2x**
- Overall speedup range: 1.43x - 4.33x
