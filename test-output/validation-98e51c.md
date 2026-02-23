# Validation Report: 98e51c1

**Commit:** 98e51c1 - Intern default tag strings to eliminate per-node malloc in loader
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 35/35 tests PASSED (100%)
- **Assertion points:** 2,480 across 27 test source files
- **YAML test suite:** 277/394 pass (failures match libyaml behavior)
- **Total test time:** 0.52 seconds

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7f644f2 |
|-----------|--------------|----------------|---------|------------|
| scan | 247.1 | 140.6 | 1.76x | -0.3% |
| parse | 231.9 | 125.7 | 1.84x | +1.0% |
| load | 165.6 | 87.3 | 1.90x | **+27.9%** |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7f644f2 |
|-----------|--------------|----------------|---------|------------|
| scan | 363.7 | 153.4 | 2.37x | +7.4% |
| parse | 331.4 | 138.5 | 2.39x | +2.0% |
| load | 223.7 | 109.2 | 2.05x | **+10.1%** |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7f644f2 |
|-----------|--------------|----------------|---------|------------|
| scan | 202.1 | 109.2 | 1.85x | -1.2% |
| parse | 177.5 | 95.6 | 1.86x | +0.5% |
| load | 116.5 | 64.2 | 1.82x | **+15.0%** |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup | vs 7f644f2 |
|-----------|--------------|----------------|---------|------------|
| scan | 89.7 | 67.1 | 1.34x | +2.5% |
| parse | 68.1 | 47.4 | 1.44x | -1.3% |
| load | 48.4 | 30.9 | 1.57x | **+16.9%** |

## Performance Impact Summary

**Tag string interning delivers massive load improvements:**
- K8s-like load: 129.5 -> 165.6 MB/s (**+27.9%**), now 1.90x vs libyaml
- Flow load: 101.3 -> 116.5 MB/s (**+15.0%**), now 1.82x vs libyaml
- Small doc load: 41.4 -> 48.4 MB/s (**+16.9%**), now 1.57x vs libyaml
- Mapping load: 203.2 -> 223.7 MB/s (**+10.1%**), now 2.05x vs libyaml

Scan and parse numbers are unchanged (expected -- this optimization targets the loader).
Load speedup vs libyaml now consistently above 1.5x across all workloads.
Mapping-heavy load crossed the 2x barrier for the first time.
