# Safety Report: afa99bf7

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** afa99bf7 (includes all fixes through 7845e16e + 995cd20d + 481524f2)

## ASAN+UBSAN (clang, Debug)

- **36/37 pass** (9.69s)
- 1 expected failure: `conformance-test-reader` (known test harness leak, not a library bug)
- **Zero ASAN heap-buffer-overflow or use-after-free errors** (in the test suite)
- **Zero UBSAN errors** (no undefined behavior detected)
- Truncation, OOM, and coverage-targets all pass (previously failing at 76ee06, fixed by 481524f2)

## Valgrind Memcheck (gcc, Debug)

- **19/19 unit tests pass** under Valgrind with zero errors, zero leaks
- Tests covered: version, reader, scanner, parser, event-api, document-api, emitter, scalars, edge-cases, differential, errors, writer, roundtrip, stress, loader-comprehensive, api-comprehensive, truncation, oom, coverage-targets
- All error-path tests (truncation, OOM, coverage-targets) now pass valgrind clean

## Fuzzing Status

- fuzz_scan: 3.5M+ runs, 0 crashes
- fuzz_parse: 3.7M+ runs, 0 crashes
- fuzz_load: **heap-use-after-free found** in `yaml_parser_load_mapping_pairs_batch`
  - Crash reproducers: `crash-a60aea3855295b5f633b2205e43db1d30c2edb6c`, `crash-a1ffd4d9a6669be4e29dd0c62bd4ab12675af6be`
  - Root cause: token queue pointer invalidation after `yaml_queue_extend` realloc
  - Not triggered by the test suite (requires specific input patterns with many flow collections)
  - **CRITICAL: still unfixed**

## Summary

| Check | Status |
|-------|--------|
| ASAN | 36/37 pass (1 known) |
| UBSAN | Clean |
| Valgrind | 19/19 pass, 0 errors, 0 leaks |
| Fuzz (scan) | 3.5M+ runs, 0 crashes |
| Fuzz (parse) | 3.7M+ runs, 0 crashes |
| Fuzz (load) | **1 use-after-free (unfixed)** |

## Verdict: PARTIAL PASS

Test suite and valgrind are fully clean. The sole remaining safety issue is the heap-use-after-free in the batch mapping loader, which is only triggered by fuzzer-generated inputs with specific patterns (flow collections causing token queue reallocation). This bug does not affect the test suite but is exploitable with crafted input.
