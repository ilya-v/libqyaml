# Validation Report: 00f72db

**Commit:** 00f72db - Add fast path for single-line plain scalar scanning
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.53 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ea9b995 |
|-----------|--------------|----------------|---------|------------|
| scan | 262.2 | 143.5 | 1.83x | +12.7% |
| parse | 228.1 | 127.3 | 1.79x | +7.2% |
| load | 134.7 | 89.2 | 1.51x | +4.3% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ea9b995 |
|-----------|--------------|----------------|---------|------------|
| scan | 370.8 | 152.5 | 2.43x | +5.4% |
| parse | 332.8 | 137.2 | 2.43x | +9.8% |
| load | 203.8 | 108.2 | 1.88x | +17.9% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ea9b995 |
|-----------|--------------|----------------|---------|------------|
| scan | 193.8 | 110.3 | 1.76x | +1.0% |
| parse | 172.9 | 97.4 | 1.78x | +7.0% |
| load | 97.9 | 64.3 | 1.52x | +10.6% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ea9b995 |
|-----------|--------------|----------------|---------|------------|
| scan | 93.1 | 67.1 | 1.39x | +16.1% |
| parse | 69.5 | 47.7 | 1.46x | +5.1% |
| load | 41.6 | 29.9 | 1.39x | +5.9% |

## Performance Improvement Summary

The fast path for single-line plain scalar scanning shows significant improvements:
- **Best improvement:** mapping-heavy load +17.9% (172.8 -> 203.8 MB/s)
- **Consistent gains:** 1-18% improvement across all workloads
- **Speedup range vs libyaml:** 1.39x - 2.43x (up from 1.14x - 2.32x)
- **Peak throughput:** 370.8 MB/s scan, 332.8 MB/s parse on mapping-heavy YAML
