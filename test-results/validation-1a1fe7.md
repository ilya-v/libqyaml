# Validation Report: b1eea41 + 1a1fe74

**Commits:**
- b1eea41 - Restore idempotent memset in delete functions and fix struct layout
- 1a1fe74 - Move possible_simple_key_count to end of yaml_parser_t struct
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
| scan | 399.5 | 151.0 | 2.65x |
| parse | 349.4 | 128.9 | 2.71x |
| load | 239.0 | 90.0 | 2.66x |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 616.7 | 154.3 | 4.00x |
| parse | 550.1 | 139.4 | 3.95x |
| load | 386.4 | 110.7 | 3.49x |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 235.5 | 115.9 | 2.03x |
| parse | 217.4 | 98.9 | 2.20x |
| load | 166.0 | 65.5 | 2.53x |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 107.2 | 69.2 | 1.55x |
| parse | 70.2 | 47.4 | 1.48x |
| load | 53.0 | 31.8 | 1.67x |

## Performance Impact Summary

Struct layout optimization -- consistent with prior commits:
- Mapping scan: 616.7 MB/s, 4.00x
- Mapping parse: 550.1 MB/s, 3.95x
- Mapping load: 386.4 MB/s, 3.49x
- K8s scan: 399.5 MB/s, 2.65x
- K8s parse: 349.4 MB/s, 2.71x
- K8s load: 239.0 MB/s, 2.66x
- **Five operations above 3x, one at 4x**
- **Eleven operations above 2x**
- Overall speedup range: 1.48x - 4.00x
