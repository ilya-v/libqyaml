# Validation Report: c341e2a

**Commit:** c341e2a - Remove unnecessary memset in yaml_string_extend
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.57 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs e183da5 |
|-----------|--------------|----------------|---------|------------|
| scan | 261.1 | 148.8 | 1.75x | +3.9% |
| parse | 234.4 | 125.5 | 1.87x | +3.1% |
| load | 168.3 | 84.0 | 2.00x | +5.3% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs e183da5 |
|-----------|--------------|----------------|---------|------------|
| scan | 364.3 | 154.7 | 2.35x | +2.3% |
| parse | 345.7 | 132.8 | 2.60x | +3.9% |
| load | 235.5 | 104.8 | 2.25x | +2.9% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs e183da5 |
|-----------|--------------|----------------|---------|------------|
| scan | 205.4 | 115.9 | 1.77x | +6.2% |
| parse | 174.0 | 98.9 | 1.76x | +2.7% |
| load | 116.9 | 62.8 | 1.86x | +6.0% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs e183da5 |
|-----------|--------------|----------------|---------|------------|
| scan | 92.9 | 69.1 | 1.34x | +1.6% |
| parse | 70.7 | 46.8 | 1.51x | +3.7% |
| load | 49.9 | 30.7 | 1.63x | +4.2% |

## Performance Impact Summary

Removing the unnecessary memset in yaml_string_extend delivers consistent improvements across all workloads:
- **K8s-like load crosses 2x barrier:** 159.9 -> 168.3 MB/s (+5.3%), now 2.00x vs libyaml
- **Mapping-heavy parse hits new peak:** 345.7 MB/s, 2.60x vs libyaml (best parse speedup yet)
- **Mapping-heavy load:** 228.8 -> 235.5 MB/s (+2.9%), 2.25x vs libyaml
- **Flow load:** 110.3 -> 116.9 MB/s (+6.0%), 1.86x vs libyaml
- Consistent +2-6% improvement across all operations and workloads
- Overall speedup range vs libyaml: 1.34x - 2.60x
- Peak throughput: 364.3 MB/s scan, 345.7 MB/s parse
