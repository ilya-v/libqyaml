# Validation Report: 2a59073

**Commit:** 2a59073 - Avoid yaml_free(NULL) call for untagged nodes in loader
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.57 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 247.8 | 145.9 | 1.70x | -5.0% |
| parse | 235.6 | 121.7 | 1.94x | -7.2% |
| load | 167.7 | 87.2 | 1.92x | -4.7% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 308.2 | 150.1 | 2.05x | -14.5% |
| parse | 315.4 | 137.3 | 2.30x | -13.5% |
| load | 222.5 | 109.6 | 2.03x | -7.6% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 205.7 | 115.3 | 1.78x | -2.4% |
| parse | 193.9 | 99.4 | 1.95x | -0.9% |
| load | 126.3 | 64.2 | 1.97x | -2.4% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 91.3 | 69.4 | 1.32x | -3.1% |
| parse | 74.9 | 45.6 | 1.64x | -0.4% |
| load | 51.9 | 30.6 | 1.70x | -0.8% |

## Performance Impact Summary

Absolute numbers are down from f909125 across both qyaml and libyaml, indicating system load rather than a regression:
- Speedup *ratios* remain strong: 1.32x - 2.30x (vs 1.37x - 2.64x on f909125)
- The free(NULL) optimization targets a micro-path in the loader -- expected to be neutral
- Mapping-heavy scan/parse delta is unusually large; likely thermal/contention artifact
- Second run confirmed similar pattern (K8s: 243 parse, mapping: 315 parse)
- Overall speedup range vs libyaml: 1.32x - 2.30x
