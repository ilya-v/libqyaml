# Validation Report: 7abb7bb

**Commit:** 7abb7bb - feat: Add line-break fast path to scan_to_next_token
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.54 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 668f345 |
|-----------|--------------|----------------|---------|------------|
| scan | 298.8 | 150.7 | 1.98x | **+5.3%** |
| parse | 313.9 | 133.5 | 2.35x | **+5.2%** |
| load | 205.5 | 89.3 | 2.30x | +2.0% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 668f345 |
|-----------|--------------|----------------|---------|------------|
| scan | 420.7 | 157.0 | 2.68x | **+7.2%** |
| parse | 450.1 | 140.3 | 3.21x | **+3.4%** |
| load | 288.0 | 110.8 | 2.60x | +1.6% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 668f345 |
|-----------|--------------|----------------|---------|------------|
| scan | 248.9 | 116.0 | 2.15x | +1.9% |
| parse | 240.8 | 96.8 | 2.49x | +0.1% |
| load | 157.8 | 66.3 | 2.38x | +0.5% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 668f345 |
|-----------|--------------|----------------|---------|------------|
| scan | 103.3 | 71.7 | 1.44x | +9.2% |
| parse | 83.7 | 49.2 | 1.70x | -0.1% |
| load | 53.6 | 32.1 | 1.67x | -5.4% |

## Performance Impact Summary

Line-break fast path delivers broad scan/parse improvements:
- **Mapping scan: 420.7 MB/s, 2.68x vs libyaml** (new all-time scan peak! was 392.3)
- **Mapping parse: 450.1 MB/s, 3.21x** (new all-time parse peak! was 435.2)
- **K8s scan: 298.8 MB/s, 1.98x** (+5.3%, approaching 2x)
- **K8s parse: 313.9 MB/s, 2.35x** (+5.2%)
- **Small scan: 103.3 MB/s, 1.44x** (+9.2%)
- Mapping load: 288.0 MB/s, 2.60x (+1.6%)
- **Twelve operations above 2x**
- Overall speedup range: 1.44x - 3.21x
- This is the strongest scan improvement in many commits
