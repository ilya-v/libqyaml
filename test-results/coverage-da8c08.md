# Coverage Analysis Report — da8c0877

**Date:** 2026-02-23
**Commit:** da8c0877
**Tool:** clang -fprofile-instr-generate -fcoverage-mapping + llvm-cov
**Corpus:** 6,041 files (706 seed files + 5,335 fuzz campaign corpus files)
**Scope:** All 3 API levels exercised: scanner, parser, loader

## Summary (Read-Path Only)

The project is a read-focused YAML parser. Write-path files (emitter.c, dumper.c, writer.c) are untouched by design — they are pass-through from libyaml and not part of the performance optimization work. API functions for emitter setup, event construction, and document building are similarly unused.

**Read-path coverage (scanner + parser + loader + reader + private headers):**

| File | Line Coverage | Branch Coverage | Function Coverage |
|------|-------------|-----------------|-------------------|
| scanner.c | 94.4% (2066/2188) | 73.8% (2576/3490) | 100% (47/47) |
| parser.c | 92.1% (824/895) | 77.2% (724/938) | 91.7% (22/24) |
| loader.c | 88.9% (667/750) | 68.1% (471/692) | 100% (18/18) |
| reader.c | 83.8% (363/433) | 80.4% (209/260) | 100% (6/6) |
| yaml_private.h | 87.5% (35/40) | 70.0% (7/10) | 100% (4/4) |
| api.c (read funcs) | ~90% (est.) | ~75% (est.) | ~85% (est.) |

**Combined read-path line coverage:** ~92% (3955/4306)
**Combined read-path function coverage:** ~98% (97/99)

## Full Library Coverage

| File | Lines | Missed | Cover | Branches | Missed | Cover | Functions | Missed | Cover |
|------|-------|--------|-------|----------|--------|-------|-----------|--------|-------|
| scanner.c | 2188 | 122 | 94.4% | 3490 | 914 | 73.8% | 47 | 0 | 100% |
| parser.c | 895 | 71 | 92.1% | 938 | 214 | 77.2% | 24 | 2 | 91.7% |
| loader.c | 750 | 83 | 88.9% | 692 | 221 | 68.1% | 18 | 0 | 100% |
| reader.c | 433 | 70 | 83.8% | 260 | 51 | 80.4% | 6 | 0 | 100% |
| yaml_private.h | 40 | 5 | 87.5% | 10 | 3 | 70.0% | 4 | 0 | 100% |
| api.c | 833 | 595 | 28.6% | 740 | 596 | 19.5% | 53 | 39 | 26.4% |
| emitter.c | 1523 | 1523 | 0.0% | 2384 | 2384 | 0.0% | 47 | 47 | 0.0% |
| dumper.c | 226 | 226 | 0.0% | 156 | 156 | 0.0% | 11 | 11 | 0.0% |
| writer.c | 79 | 79 | 0.0% | 46 | 46 | 0.0% | 2 | 2 | 0.0% |
| **TOTAL** | **6967** | **2774** | **60.2%** | **8716** | **4585** | **47.4%** | **212** | **101** | **52.4%** |

## Uncovered Code Analysis

### scanner.c (122 missed lines)
- **~80 lines:** OOM error paths (malloc failure → `YAML_MEMORY_ERROR` → return 0). Unreachable without malloc interception which conflicts with coverage tooling.
- **~15 lines:** Batch scanner OOM cleanup (`yaml_free(kval)`, `yaml_free(vval)`, return -1). Same OOM category.
- **~10 lines:** Rare encoding paths (CR-only line breaks at line 1436, specific BOM handling).
- **~10 lines:** Simple key edge cases (`sk->possible = 0`, `parser->possible_simple_key_count--`).
- **~7 lines:** Empty token fast path (line 933-935: `memset(token, 0, sizeof(yaml_token_t))`).

### parser.c (71 missed lines)
- **~40 lines:** OOM error paths.
- **~20 lines:** Error recovery and cleanup.
- **2 functions missed:** Likely `yaml_parser_parse_flow_mapping_key` and `yaml_parser_parse_flow_mapping_value` edge cases.
- **~9 lines:** Assert/unreachable paths.

### loader.c (83 missed lines)
- **~50 lines:** OOM error paths and cleanup (`goto error`, `yaml_free(tag)`, `STACK_DEL`).
- **~15 lines:** Alias resolution error paths.
- **~10 lines:** Assert/unreachable paths (`assert(0); /* Could not happen */`).
- **~8 lines:** Multi-document load edge cases.

### reader.c (70 missed lines)
- **~50 lines:** UTF-16/UTF-32 decoding paths (our test inputs are all UTF-8).
- **~10 lines:** Raw buffer management edge cases.
- **~10 lines:** OOM in buffer allocation.

### api.c (595 missed lines)
- **~400 lines:** Emitter API functions (0% covered, expected).
- **~100 lines:** Event construction functions (unused in read path).
- **~50 lines:** Document construction functions (unused in read path).
- **~45 lines:** Remaining read-path functions with partial coverage.

## Recommendations

1. **UTF-16/UTF-32 reader coverage:** Write fuzz inputs with BOM-prefixed UTF-16LE, UTF-16BE, and UTF-32 encodings to cover reader.c's encoding paths. This would add ~50 lines of coverage.

2. **OOM injection:** The largest uncovered category (~180 lines) is OOM error paths. A malloc-intercepting OOM fuzzer would cover these, but conflicts with coverage tooling. Consider a separate OOM-injection pass without coverage measurement.

3. **Emitter/dumper coverage:** These are write-path files (0% covered) and are NOT part of the performance optimization scope. Including them would require an emitter fuzz harness, which is low priority since we're focused on read-path correctness and performance.

4. **Branch coverage gap:** Branch coverage (73-80% on read path) is lower than line coverage because many branches are error-path-only. The achievable ceiling without OOM injection is approximately 80-85%.

## Artifacts

- Profile data: `test-output/cov-da8c08.profdata`
- Coverage driver: `fuzz/coverage_driver.c`
