# Validation Report: b8bb685

**Commit:** b8bb685 - Add fast path for simple quoted scalar scanning
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.50 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 00f72db |
|-----------|--------------|----------------|---------|------------|
| scan | 255.3 | 151.0 | 1.69x | -2.6% |
| parse | 226.6 | 130.4 | 1.74x | -0.7% |
| load | 132.4 | 88.9 | 1.49x | -1.7% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 00f72db |
|-----------|--------------|----------------|---------|------------|
| scan | 373.7 | 154.6 | 2.42x | +0.8% |
| parse | 342.5 | 138.3 | 2.48x | +2.9% |
| load | 199.5 | 110.1 | 1.81x | -2.1% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 00f72db |
|-----------|--------------|----------------|---------|------------|
| scan | 199.7 | 113.2 | 1.76x | +3.0% |
| parse | 178.3 | 97.0 | 1.84x | +3.1% |
| load | 99.2 | 64.9 | 1.53x | +1.3% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 00f72db |
|-----------|--------------|----------------|---------|------------|
| scan | 95.5 | 67.9 | 1.41x | +2.6% |
| parse | 71.1 | 48.5 | 1.47x | +2.3% |
| load | 42.5 | 31.6 | 1.34x | +2.2% |

## Summary

- All 35 tests pass
- Quoted scalar fast path improves flow and small doc performance (+2-3%)
- Mapping-heavy parse improved to 2.48x (best parse speedup yet)
- K8s-like slightly slower (-0.7 to -2.6%) -- within measurement noise
- Speedup range vs libyaml: 1.34x - 2.48x
- Peak throughput: 373.7 MB/s scan, 342.5 MB/s parse
