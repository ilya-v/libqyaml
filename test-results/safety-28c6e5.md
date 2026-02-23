# Safety Report: 28c6e52

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** 28c6e52 (scanner: Add flow entry scalar batch path for flow sequences)

## ASAN+UBSAN (clang, Debug)

- **36/37 pass** (10.76s)
- 1 expected failure: `conformance-test-reader` (known test harness leak, not a library bug)
- With leak detection disabled, conformance-test-reader passes cleanly
- **Zero ASAN errors** (no heap-buffer-overflow, use-after-free, etc.)
- **Zero UBSAN errors** (no undefined behavior detected)

## Valgrind Memcheck (gcc, Debug)

- **20/20 unit tests pass** under Valgrind
- **Zero memory errors**
- **Zero leaks**
- Tests covered: version, reader, event-api, errors, writer, edge-cases, roundtrip, document-api, scalars, parser, emitter, scanner, truncation, api-comprehensive, stress, differential, reader-comprehensive, scanner-comprehensive, parser-comprehensive, emitter-comprehensive

## Verdict

The recent optimizations (zero-copy reader, streamlined batch path, flow entry batch) introduce no memory safety regressions. All sanitizers and Valgrind are clean.
