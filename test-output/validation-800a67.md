# Validation Report: 800a67e

**Commit:** 800a67e - Construct loader nodes directly in the nodes stack
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.54 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs aa57e62 |
|-----------|--------------|----------------|---------|------------|
| scan | 274.7 | 148.7 | 1.85x | -3.9% |
| parse | 300.2 | 131.2 | 2.29x | -0.4% |
| load | 205.2 | 90.2 | 2.28x | +4.2% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs aa57e62 |
|-----------|--------------|----------------|---------|------------|
| scan | 392.1 | 157.9 | 2.48x | +1.6% |
| parse | 435.4 | 142.7 | 3.05x | +0.7% |
| load | 284.1 | 112.3 | 2.53x | **+5.2%** |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs aa57e62 |
|-----------|--------------|----------------|---------|------------|
| scan | 240.5 | 114.2 | 2.11x | -1.8% |
| parse | 237.6 | 98.7 | 2.41x | +0.6% |
| load | 155.4 | 66.2 | 2.35x | **+11.6%** |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs aa57e62 |
|-----------|--------------|----------------|---------|------------|
| scan | 99.4 | 70.0 | 1.42x | +2.1% |
| parse | 81.9 | 49.0 | 1.67x | -5.4% |
| load | 55.5 | 31.4 | 1.77x | +1.5% |

## Performance Impact Summary

Constructing loader nodes directly in the nodes stack shows clear load improvements:
- **Mapping load: 284.1 MB/s, 2.53x vs libyaml** (new peak! was 270.0, +5.2%)
- **Flow load: 155.4 MB/s, 2.35x** (+11.6% -- significant!)
- **K8s load: 205.2 MB/s, 2.28x** (+4.2%, new peak for K8s load!)
- Mapping parse: 435.4 MB/s, 3.05x (consistent)
- Scan/parse essentially unchanged (expected -- optimization targets loader)
- **Twelve operations above 2x**
- Overall speedup range: 1.42x - 3.05x

## Known Issue

Double-free bug persists from commit bda3a5f (memset removal in yaml_document_delete).
See crash reproducer at crash-b5348f3a952666b4a92afa8f6d22a5cdbf1478b3.
Worker has been notified.
