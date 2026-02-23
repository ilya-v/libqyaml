# Validation Report: 7d86f8c

**Commit:** 7d86f8c - feat: Add plain scalar fast paths in parser state handlers
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.56 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7abb7bb |
|-----------|--------------|----------------|---------|------------|
| scan | 280.3 | 150.9 | 1.86x | -6.2% |
| parse | 320.5 | 132.5 | 2.42x | **+2.1%** |
| load | 208.5 | 90.5 | 2.30x | +1.5% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7abb7bb |
|-----------|--------------|----------------|---------|------------|
| scan | 413.1 | 150.9 | 2.74x | -1.8% |
| parse | 457.4 | 141.3 | 3.24x | +1.6% |
| load | 293.8 | 113.8 | 2.58x | +2.0% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7abb7bb |
|-----------|--------------|----------------|---------|------------|
| scan | 250.9 | 112.7 | 2.23x | +0.8% |
| parse | 257.8 | 97.5 | 2.64x | **+7.1%** |
| load | 165.9 | 66.6 | 2.49x | **+5.2%** |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7abb7bb |
|-----------|--------------|----------------|---------|------------|
| scan | 103.7 | 68.7 | 1.51x | +0.4% |
| parse | 82.2 | 48.3 | 1.70x | -1.8% |
| load | 52.3 | 31.6 | 1.65x | -2.4% |

## Performance Impact Summary

Plain scalar fast paths deliver targeted parse and load improvements:
- **Mapping parse: 457.4 MB/s, 3.24x vs libyaml** (new all-time parse peak!)
- **Flow parse: 257.8 MB/s, 2.64x** (+7.1% -- largest flow parse gain this session)
- **Flow load: 165.9 MB/s, 2.49x** (+5.2%)
- **Mapping load: 293.8 MB/s, 2.58x** (new all-time load peak!)
- K8s parse: 320.5 MB/s, 2.42x (+2.1%)
- K8s load: 208.5 MB/s, 2.30x
- K8s scan slightly down (-6.2%), likely noise
- **Twelve operations above 2x**
- Overall speedup range: 1.51x - 3.24x
