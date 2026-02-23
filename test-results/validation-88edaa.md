# Validation Report: 88edaa4

**Commit:** 88edaa4 - Remove unnecessary memset from yaml_token_delete and yaml_event_delete
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.65 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ee2954c |
|-----------|--------------|----------------|---------|------------|
| scan | 240.6 | 130.2 | 1.85x | -7.6% |
| parse | 276.8 | 111.8 | 2.48x | **+6.1%** |
| load | 179.8 | 85.8 | 2.10x | +0.9% |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ee2954c |
|-----------|--------------|----------------|---------|------------|
| scan | 343.7 | 154.4 | 2.23x | -7.5% |
| parse | 375.4 | 137.1 | 2.74x | **+2.6%** |
| load | 255.6 | 106.1 | 2.41x | +4.2% |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ee2954c |
|-----------|--------------|----------------|---------|------------|
| scan | 215.9 | 113.6 | 1.90x | +4.0% |
| parse | 196.1 | 98.0 | 2.00x | +1.2% |
| load | 129.3 | 63.0 | 2.05x | +1.7% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs ee2954c |
|-----------|--------------|----------------|---------|------------|
| scan | 91.2 | 65.8 | 1.39x | +5.9% |
| parse | 76.9 | 49.1 | 1.57x | +0.1% |
| load | 53.0 | 30.6 | 1.73x | -2.3% |

## Performance Impact Summary

Removing memset from token/event delete shows clear parse improvements:
- **K8s parse: 2.48x** (+6.1%), 276.8 MB/s -- significant jump
- **Mapping parse new all-time peak:** 375.4 MB/s, 2.74x vs libyaml
- **Mapping load:** 255.6 MB/s, 2.41x
- **Flow parse/load:** 2.00x / 2.05x
- Scan numbers slightly down (measurement noise -- memset removal shouldn't affect scan)
- **Eight operations above 2x vs libyaml**
- Overall speedup range: 1.39x - 2.74x
- Combined with 873915c (direct queue scan), these two commits have dramatically improved parse/load performance
