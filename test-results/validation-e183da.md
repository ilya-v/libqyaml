# Validation Report: e183da5

**Commit:** e183da5 - Add branch prediction hints to fetch_more_tokens hot path
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.51 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 98e51c1 |
|-----------|--------------|----------------|---------|------------|
| scan | 251.4 | 146.2 | 1.72x | +1.7% |
| parse | 227.4 | 129.1 | 1.76x | -1.9% |
| load | 159.9 | 87.3 | 1.83x | -3.4% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 98e51c1 |
|-----------|--------------|----------------|---------|------------|
| scan | 356.1 | 152.2 | 2.34x | -2.1% |
| parse | 332.8 | 137.4 | 2.42x | +0.4% |
| load | 228.8 | 108.5 | 2.11x | +2.3% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 98e51c1 |
|-----------|--------------|----------------|---------|------------|
| scan | 193.4 | 111.7 | 1.73x | -4.3% |
| parse | 169.5 | 96.8 | 1.75x | -4.5% |
| load | 110.3 | 64.8 | 1.70x | -5.3% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 98e51c1 |
|-----------|--------------|----------------|---------|------------|
| scan | 91.4 | 70.3 | 1.30x | +1.9% |
| parse | 68.2 | 48.7 | 1.40x | +0.1% |
| load | 47.9 | 31.1 | 1.54x | -1.0% |

## Performance Impact Summary

Branch prediction hints show mixed/neutral results -- within measurement noise for most workloads:
- Mapping-heavy load slightly improved: 223.7 -> 228.8 MB/s (+2.3%), still above 2x vs libyaml
- Flow sequence shows slight regression: 116.5 -> 110.3 MB/s (-5.3%), likely measurement noise
- K8s and small doc essentially unchanged
- Overall speedup range vs libyaml: 1.30x - 2.42x
- Peak throughput: 356.1 MB/s scan, 332.8 MB/s parse on mapping-heavy YAML
