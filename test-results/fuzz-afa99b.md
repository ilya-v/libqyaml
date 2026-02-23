# Fuzz Campaign Summary: afa99bf7

**Date:** 2026-02-23
**Tested by:** tester agent
**Commit:** afa99bf7 (includes all code through 7845e16e + bug fixes)
**Engine:** libFuzzer with ASAN (clang)

## Campaign Results

| Harness | Total Runs | Exec/sec | Corpus Size | Crashes |
|---------|-----------|----------|-------------|---------|
| fuzz_scan | ~6.2M | 43-58K | 13,619 | 0 |
| fuzz_parse | ~3.7M | 60K | 7,586 | 0 |
| fuzz_load | ~2.7M | 44-55K | 6,782 | 2 (use-after-free) |
| fuzz_structured | ~4.0M | 66K | 2,071 | 0 |
| **Total** | **~16.6M** | | **30,058** | **2** |

## Crashes Found

### 1. heap-use-after-free (CRITICAL)
- **Crash files:** `crash-a60aea3855295b5f633b2205e43db1d30c2edb6c`, `crash-a1ffd4d9a6669be4e29dd0c62bd4ab12675af6be`
- **Location:** `yaml_parser_load_mapping_pairs_batch` (loader.c:937-976)
- **Root cause:** Token queue pointers (`key_scalar`, `value_scalar`) become dangling after `yaml_queue_extend` reallocs the token buffer during a subsequent `LOADER_PEEK_TOKEN` call
- **Trigger:** Flow-style YAML with many nested collections causing token queue growth
- **Status:** UNFIXED
- **Severity:** Critical -- reads freed memory, exploitable with crafted input

### 2. Memory leak (FIXED)
- **Crash file:** `leak-3fa0e5054cc5e21e08893164b47fdbf7e9d90ed7`
- **Location:** `yaml_parser_load_mapping_pairs_batch` error paths
- **Root cause:** Scanner-allocated scalar strings not freed on error exits
- **Status:** Fixed in commit 481524f2
- **Verified:** All error-path tests pass valgrind clean post-fix

### 3. Historical double-free (FIXED in prior session)
- **Crash file:** `crash-b5348f3a952666b4a92afa8f6d22a5cdbf1478b3`
- **Root cause:** memset removal in yaml_document_delete
- **Status:** Fixed in prior session

## Coverage Notes

- Scanner and parser APIs are thoroughly fuzzed with no crashes
- Load API has the one outstanding use-after-free in the batch mapping optimization
- Structure-aware fuzzer generates valid YAML constructs and finds no issues
- All corpus inputs are saved in `test-output/fuzz/corpus_*/` for regression testing
