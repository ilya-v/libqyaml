# Validation Report: 3d77566

**Commit:** 3d77566 - Fix -Wshadow warnings in emitter by renaming shadowed locals
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.55 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 397.6 | 152.5 | 2.61x |
| parse | 405.2 | 127.7 | 3.17x |
| load | 237.2 | 90.0 | 2.64x |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 615.2 | 157.8 | 3.90x |
| parse | 637.1 | 141.8 | 4.49x |
| load | 381.3 | 111.6 | 3.42x |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 237.7 | 113.7 | 2.09x |
| parse | 255.1 | 97.2 | 2.62x |
| load | 166.8 | 65.9 | 2.53x |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 102.2 | 70.4 | 1.45x |
| parse | 80.2 | 48.9 | 1.64x |
| load | 54.2 | 31.4 | 1.73x |

## Performance Impact Summary

Warning fix commit -- performance unchanged as expected (cleaner run than 5c15fed):
- **K8s parse: 405.2 MB/s, 3.17x** (K8s parse now above 3x!)
- **K8s scan: 397.6 MB/s, 2.61x** (new K8s peak)
- **K8s load: 237.2 MB/s, 2.64x** (new K8s peak)
- Mapping parse: 637.1 MB/s, 4.49x
- Mapping scan: 615.2 MB/s, 3.90x
- Mapping load: 381.3 MB/s, 3.42x
- Flow load: 166.8 MB/s, 2.53x
- **Five operations above 3x, one above 4x**
- **Twelve operations above 2x**
- Overall speedup range: 1.45x - 4.49x
