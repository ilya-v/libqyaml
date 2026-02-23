# Validation Report: 7f644f2

**Commit:** 7f644f2 - Add fast path in scan_to_next_token for inline whitespace
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.49 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 247.8 | 146.4 | 1.69x |
| parse | 229.5 | 129.9 | 1.77x |
| load | 129.5 | 86.5 | 1.50x |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 338.5 | 155.6 | 2.18x |
| parse | 325.0 | 141.0 | 2.30x |
| load | 203.2 | 111.3 | 1.83x |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 204.5 | 113.0 | 1.81x |
| parse | 176.7 | 96.4 | 1.83x |
| load | 101.3 | 66.6 | 1.52x |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 87.5 | 70.7 | 1.24x |
| parse | 69.0 | 48.9 | 1.41x |
| load | 41.4 | 31.5 | 1.31x |

## Summary

- All 35 tests pass
- Speedup range vs libyaml: 1.24x - 2.30x
- Performance roughly neutral vs previous commit (within measurement noise)
- Flow sequence shows slight improvement (+2.4% scan, -0.9% parse)
- Peak throughput: 338.5 MB/s scan, 325.0 MB/s parse
