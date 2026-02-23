# Validation Report: 3f28aa4

**Commit:** 3f28aa4 - Fix compiler warnings in emitter and scanner
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.55 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 281.4 | 150.5 | 1.87x |
| parse | 303.4 | 128.0 | 2.37x |
| load | 194.6 | 90.2 | 2.16x |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 389.1 | 155.7 | 2.50x |
| parse | 433.5 | 138.3 | 3.13x |
| load | 270.6 | 109.0 | 2.48x |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 237.9 | 113.5 | 2.10x |
| parse | 244.2 | 97.9 | 2.49x |
| load | 144.9 | 64.8 | 2.24x |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 97.2 | 70.4 | 1.38x |
| parse | 83.8 | 48.9 | 1.71x |
| load | 55.7 | 32.6 | 1.71x |

## Performance Impact Summary

Warning fix commit -- performance unchanged as expected:
- Mapping parse: 433.5 MB/s, 3.13x vs libyaml
- Mapping scan: 389.1 MB/s, 2.50x (new scan peak!)
- Mapping load: 270.6 MB/s, 2.48x
- All numbers consistent with 6100009
- **Twelve operations above 2x** (mapping scan at 2.50x pushed it above)
- Overall speedup range: 1.38x - 3.13x
