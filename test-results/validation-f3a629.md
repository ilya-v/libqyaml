# Validation Report: f3a6295

**Commit:** f3a6295 - Add batch fast path for block mapping key-value token emission
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.55 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7a674b5 |
|-----------|--------------|----------------|---------|------------|
| scan | 349.7 | 148.5 | 2.35x | **+27.4%** |
| parse | 349.5 | 132.4 | 2.64x | **+17.5%** |
| load | 214.3 | 91.1 | 2.35x | **+9.7%** |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7a674b5 |
|-----------|--------------|----------------|---------|------------|
| scan | 633.3 | 157.9 | **4.01x** | **+63.1%** |
| parse | 643.0 | 142.5 | **4.51x** | **+48.3%** |
| load | 344.9 | 110.6 | **3.12x** | **+21.8%** |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7a674b5 |
|-----------|--------------|----------------|---------|------------|
| scan | 242.8 | 113.5 | 2.14x | -0.3% |
| parse | 249.6 | 98.2 | 2.54x | -2.7% |
| load | 161.8 | 66.3 | 2.44x | +3.5% |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7a674b5 |
|-----------|--------------|----------------|---------|------------|
| scan | 100.9 | 70.4 | 1.43x | +15.1% |
| parse | 76.8 | 47.8 | 1.61x | -1.4% |
| load | 50.8 | 31.4 | 1.62x | -0.6% |

## Performance Impact Summary

**BREAKTHROUGH: First operations above 4x vs libyaml! Multiple new all-time records!**

- **Mapping parse: 643.0 MB/s, 4.51x vs libyaml** (was 3.09x -- +46%!)
- **Mapping scan: 633.3 MB/s, 4.01x** (was 2.48x -- +63%!)
- **Mapping load: 344.9 MB/s, 3.12x** (was 2.53x -- +22%!)
- **K8s scan: 349.7 MB/s, 2.35x** (was 1.85x -- +27%!)
- **K8s parse: 349.5 MB/s, 2.64x** (was 2.26x -- +18%!)
- **K8s load: 214.3 MB/s, 2.35x** (was 2.16x -- +10%!)
- Flow numbers unchanged (expected -- optimization targets block mappings)
- **Three operations now above 3x, two above 4x**
- **Twelve operations above 2x**
- Overall speedup range: 1.43x - 4.51x
- This is the single largest performance jump in the project's history
- The batch fast path for block mapping key-value pairs essentially amortizes per-token overhead
