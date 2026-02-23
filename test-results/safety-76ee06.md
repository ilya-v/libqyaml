# Safety Report: 76ee064a

**Date:** 2026-02-23
**Tested by:** tester agent
**Commits:** 76ee064a (node array prealloc) + 3d8e00c1 (batch mapping pairs + pointer iteration)

## ASAN+UBSAN (clang, Debug)

- **33/37 pass** (9.69s)
- 4 failures:
  - `truncation` -- **NEW: 463 bytes leaked in 88 allocations** (error-path leak in batch mapping code)
  - `coverage-targets` -- **NEW: 4 bytes leaked in 1 allocation** (error-path leak on load failure)
  - `oom` -- **NEW: 147 bytes leaked in 22 allocations** (error-path leak on allocation failure)
  - `conformance-test-reader` -- pre-existing known test harness leak
- **Zero ASAN heap-buffer-overflow or use-after-free errors**
- **Zero UBSAN errors** (no undefined behavior detected)

## Valgrind Memcheck (gcc, Debug)

- **16/16 core unit tests pass** under Valgrind with zero errors, zero leaks:
  version, reader, scanner, parser, event-api, document-api, emitter, scalars, edge-cases, differential, errors, writer, roundtrip, stress, loader-comprehensive, api-comprehensive
- **truncation test: FAILS** -- 463 bytes definitely lost in 88 blocks
- **oom test: FAILS** -- 147 bytes definitely lost in 22 blocks
- **coverage-targets test: FAILS** -- 4 bytes definitely lost in 1 block

## Root Cause Analysis

The leaks are **introduced by commit 3d8e00c1** (batch mapping pairs). Verified by testing at e09ef672 (previous commit) where truncation test passes valgrind clean with zero leaks.

The batch mapping code in `loader.c:yaml_parser_load_mapping_pairs_batch()` has multiple error exit points (`return 0`) that do not properly free scanner-allocated scalar string values. When `LOADER_SKIP_TOKEN` advances past a scalar token, the scanner-allocated string (`token->data.scalar.value`) is still live. If the batch code then fails (e.g., `yaml_arena_alloc` returns NULL, or `LOADER_PEEK_TOKEN` fails on a subsequent fetch), the string is orphaned -- the token is already consumed (so parser cleanup won't free it) but the batch code never freed it.

Specific leak sites (all in `yaml_parser_load_mapping_pairs_batch`):
1. Line 695: `key_scalar = LOADER_PEEK_TOKEN()` returns NULL after KEY token was skipped -- key token string leaked
2. Line 705: `token = LOADER_PEEK_TOKEN()` returns NULL after key scalar was skipped -- key scalar string leaked
3. Line 713: `value_scalar = LOADER_PEEK_TOKEN()` returns NULL after VALUE token was skipped -- key scalar string leaked
4. Line 738/762: `yaml_arena_alloc()` returns NULL -- key/value scalar strings leaked
5. Line 730/755: `STACK_PUSH_RESERVE` fails -- key/value scalar strings leaked

## Severity

**Medium.** The leaks occur only on error paths (truncated input, OOM conditions, malformed input that causes fetch failures). Normal parsing of valid YAML is clean. However, the library is supposed to handle all error paths gracefully per the requirements (Section 4.5, 4.6, 4.7).

## Recommendation

The worker should add cleanup code to the error exits in `yaml_parser_load_mapping_pairs_batch()` to free scanner-allocated scalar strings when the batch path fails mid-processing. The fix should track which scalar tokens have been consumed (SKIP'd) but not yet transferred to arena nodes.

## Verdict: PARTIAL PASS

Normal code paths are clean (zero leaks, zero memory errors). Error paths have regressions from the batch mapping optimization. This needs to be fixed.
