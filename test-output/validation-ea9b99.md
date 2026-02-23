# Validation Report: ea9b995

**Commit:** ea9b995 - Move token memset to error/early-exit paths in yaml_parser_scan
**Date:** 2026-02-23
**Validated by:** tester agent

## Unit Tests

- **Result:** 33/33 tests PASSED (100%)
- **Assertion points:** 2,409 across 25 test source files
- **YAML test suite:** 277/394 pass (206 valid + 71 error); failures match libyaml behavior
- **Total test time:** 0.50 seconds

### Test Breakdown
| Test | Status |
|------|--------|
| version | PASS |
| reader | PASS |
| scanner | PASS |
| parser | PASS |
| event-api | PASS |
| document-api | PASS |
| emitter | PASS |
| scalars | PASS |
| edge-cases | PASS |
| differential | PASS |
| errors | PASS |
| stress | PASS |
| writer | PASS |
| roundtrip | PASS |
| scanner-comprehensive | PASS |
| parser-comprehensive | PASS |
| api-comprehensive | PASS |
| reader-comprehensive | PASS |
| errors-comprehensive | PASS |
| emitter-comprehensive | PASS |
| loader-comprehensive | PASS |
| truncation | PASS |
| deep-nesting | PASS |
| oom | PASS |
| differential-libyaml | PASS |
| conformance-test-version | PASS |
| conformance-test-reader | PASS |
| yaml-test-suite-parser | PASS |
| conformance-run-scanner | PASS |
| conformance-run-parser | PASS |
| conformance-run-loader | PASS |
| conformance-run-emitter | PASS |
| conformance-run-dumper | PASS |

## Benchmark Results (Release -O2)

### Kubernetes-like YAML (50.0 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 232.6 | 148.9 | 1.56x |
| parse | 212.7 | 130.6 | 1.63x |
| load | 129.1 | 90.2 | 1.43x |

### Mapping-heavy YAML (244.1 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 351.9 | 151.4 | 2.32x |
| parse | 303.2 | 138.4 | 2.19x |
| load | 172.8 | 109.8 | 1.57x |

### Flow sequence (57.5 KB)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 191.9 | 113.7 | 1.69x |
| parse | 161.6 | 98.3 | 1.64x |
| load | 88.5 | 66.2 | 1.34x |

### Small document (35 bytes)
| Operation | qyaml (MB/s) | libyaml (MB/s) | Speedup |
|-----------|--------------|----------------|---------|
| scan | 80.2 | 70.5 | 1.14x |
| parse | 66.1 | 48.7 | 1.36x |
| load | 39.3 | 31.8 | 1.24x |

## Summary

- All 33 tests pass with zero failures
- Performance range: 1.14x to 2.32x vs libyaml across workloads
- Best performance on mapping-heavy workloads (scan: 2.32x, parse: 2.19x)
- Lowest speedup on small documents (1.14x-1.36x) due to per-document overhead
- Peak throughput: 351.9 MB/s scan, 303.2 MB/s parse on mapping-heavy YAML
