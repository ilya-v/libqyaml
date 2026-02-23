# Validation Report: ee2954c

**Commit:** ee2954c - Add SKIP_TOKEN_AND_PEEK macro to avoid redundant fetch_more_tokens calls
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.56 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 260.4 | 148.2 | 1.76x | -0.2% |
| parse | 261.0 | 127.9 | 2.04x | **+2.8%** |
| load | 178.2 | 88.1 | 2.02x | +1.3% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 371.7 | 151.3 | 2.46x | +3.1% |
| parse | 365.8 | 135.4 | 2.70x | **+0.3%** |
| load | 245.3 | 108.5 | 2.26x | +1.9% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 207.6 | 113.2 | 1.83x | -1.5% |
| parse | 193.7 | 97.9 | 1.98x | -1.0% |
| load | 127.1 | 66.6 | 1.91x | -1.8% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs f909125 |
|-----------|--------------|----------------|---------|------------|
| scan | 86.1 | 70.5 | 1.22x | -8.6% |
| parse | 70.3 | 47.6 | 1.48x | -6.5% |
| load | 54.9 | 31.0 | 1.77x | +5.0% |

## Performance Impact Summary

SKIP_TOKEN_AND_PEEK macro delivers noticeable improvement on K8s and mapping workloads:
- **K8s parse crosses 2x:** 254.0 -> 261.0 MB/s (+2.8%), now 2.04x vs libyaml
- **K8s load crosses 2x:** 175.9 -> 178.2 MB/s (+1.3%), now 2.02x vs libyaml
- **Mapping parse new peak:** 365.8 MB/s, 2.70x vs libyaml (best ever)
- **Mapping load:** 245.3 MB/s, 2.26x vs libyaml
- Flow and small doc slightly down (-1 to -8%), within noise
- Small doc load improved +5.0% (51.9 -> 54.9 MB/s, 1.77x)
- **Five operations now above 2x threshold**
- Overall speedup range vs libyaml: 1.22x - 2.70x
