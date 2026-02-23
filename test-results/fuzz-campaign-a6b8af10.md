# Fuzzing Campaign Report — a6b8af10

**Date:** 2026-02-23T14:29:06Z
**Commit:** a6b8af10 (build: Add automated fuzzing campaign script and fix bac0cb report)
**Full hash:** a6b8af10230030036eadaf8245736c70ad9da128
**Duration:** 30 minutes (1392s wall clock, 3s build)
**Fuzzer:** libFuzzer (LLVM) with ASAN, fork=3 per harness
**Seed sources:** yaml-test-suite, yaml-test-data, yaml-fuzz (4814 seed files)
**Max input length:** 8192 bytes

## Summary

| Metric | Value |
|--------|-------|
| Harnesses run | 4 |
| Total inputs tested | 68503183 |
| Total crash artifacts | 49 |
| Differential divergences | 48 |
| Campaign duration | 1392s |
| Build time | 3s |

## Per-Harness Results

| Harness | Result | Executions | Crashes | Corpus | Coverage | Time |
|---------|--------|------------|---------|--------|----------|------|
| fuzz_scan | PASS | 24079919 | 0 | 3048 | 1929 | 460s |
| fuzz_parse | PASS | 24520579 | 0 | 1900 | 2274 | 461s |
| fuzz_load | PASS | 19902535 | 0 | 3371 | 2663 | 460s |
| fuzz_differential | FAIL | 150 | 49 | 65 | 1649 | 11s |

**Notes on per-harness results:**
- **fuzz_scan, fuzz_parse, fuzz_load**: Single-library crash-finding harnesses with ASAN. Any crash here is a memory safety bug.
- **fuzz_differential**: Compares libqyaml output against reference libyaml at scanner, parser, and loader levels. Any divergence (crash artifact) is a correctness bug. A single divergent input may diverge at multiple API levels (e.g., scanner divergence causes parser divergence too), so the divergence count below reflects unique divergent inputs, not total API-level failures.

## Differential Divergence Classification

Total unique divergent inputs: 48

| Category | Count | Description |
|----------|-------|-------------|
| PERMISSIVE | 12 | libqyaml accepts, reference rejects |
| RESTRICTIVE | 23 | libqyaml rejects, reference accepts |
| TOKEN_TYPE | 6 | Both scan successfully but produce different token types |
| SCALAR_VALUE | 7 | Both scan successfully but produce different scalar content |

**Overlap note:** Each input is classified into exactly one category based on the first divergence detected at the scanner level. There is no double-counting — total = PERMISSIVE + RESTRICTIVE + TOKEN_TYPE + SCALAR_VALUE.

### Detailed Non-BOM, Non-% Divergences (first 20)

```
=== DETAILED: Non-BOM, Non-% Divergences ===

[1] crash-aaeadf66685b3ad60f0aebdb4d3b9b95e4012a4c (SCALAR_VALUE) tok#7, 22 bytes
  Our: scalar_len=1
  Ref: scalar_len=4
  Input: ---\ndocey: , 2,}}\nue,\n

[2] crash-65eeb0857d54f3699a0d8b97fdcb1707a9ede257 (SCALAR_VALUE) tok#7, 27 bytes
  Our: scalar_len=1
  Ref: scalar_len=2
  Input: anchor: &re , 3]\nA - *list 

[3] crash-c599c3f22e840edac8ab2d9b85169326a0e1e308 (SCALAR_VALUE) tok#9, 29 bytes
  Our: scalar_len=3
  Ref: scalar_len=9
  Input: {k, \t\t\t\t\t\t\t\t2, 3], lue, lis}\n

[4] crash-afa2054b1a7199657e3d1abfc4b6c682de54bee5 (PERMISSIVE) tok#7, 30 bytes
  Ref: found a tab character that violates indentation at 3:0
  Input: ---\n-\ndoc2: value2\n\tme\nta%TAda

[5] crash-083e151c5475b98f84a4fd4482a1b91cd4b283d9 (RESTRICTIVE) tok#9, 32 bytes
  Our: did not find expected comment or line break at 2:2
  Input: ? [c, key]\n: opclm[key\n> emptyo\n

[6] crash-d64bcf5cfdd9090f28da87113075e2f9bfbc7f3c (PERMISSIVE) tok#8, 32 bytes
  Ref: could not find expected ':' at 2:0
  Input: - e*mbool: true\nnul!\n-ue\n- null\n

[7] crash-49c3d25d3710f67a84428a13a1a80ab1e8ee9a2e (PERMISSIVE) tok#8, 34 bytes
  Ref: found a tab character that violates indentation at 4:0
  Input: ---\nd\n---\ndoc2: value2\n\tme\nta%TAda

[8] crash-c85642f82f91e0e4c43cec56eb8c9c221ad77a87 (RESTRICTIVE) tok#9, 34 bytes
  Our: could not find expected ':' at 5:0
  Input: - item1\n- item2\n- 3\n- true\n-/null\n

[9] crash-9797c03b9b3eea42b0bf9076f7617f6cd863108a (RESTRICTIVE) tok#5, 35 bytes
  Our: could not find expected ':' at 3:0
  Input: - item1\n- item2\n-] tr] tr*e\n- null\n

[10] crash-359101e73331404da357c64856f0b97b725920f2 (RESTRICTIVE) tok#7, 35 bytes
  Our: could not find expected ':' at 4:0
  Input: - item1\n- item2\n- 3\n-] tr*e\n- null\n

[11] crash-d5905fc45bdc2a98459cd9ab674e9f16001c2018 (RESTRICTIVE) tok#7, 37 bytes
  Our: could not find expected ':' at 4:0
  Input: - item1\n- itemtem2\n- 3\n-- ite\n- null\n

[12] crash-f8380d200df093ff6495a014b8341ecf7640fc95 (PERMISSIVE) tok#8, 38 bytes
  Ref: found a tab character that violates indentation at 4:0
  Input: ---\ndoue1\n---\ndoc2: value2\n\tme\nta%TAda

[13] crash-2b64d5980d5c124591987ef0ebd6f709d0102e38 (RESTRICTIVE) tok#6, 39 bytes
  Our: could not find expected ':' at 5:0
  Input: ?-l%TAk_l00:(:\n-\n [ T&vcA]YAML--\ndoc3\n\n

[14] crash-22bf2731485f2fa3605d67d78011604ed7ea48c3 (PERMISSIVE) tok#3, 40 bytes
  Ref: could not find expected ':' at 2:0
  Input: ? [complex,, key]\n.\ny\n: key]\nplexey too\n

[15] crash-935f4b1a2d87234df78831faa3e3ec0d722f04a7 (SCALAR_VALUE) tok#19, 41 bytes
  Our: scalar_len=5
  Ref: scalar_len=7
  Input: {key: value,: [1, 2, 3], nes 3], nestb}}\n

[16] crash-c2b8f30d6d9870c6e63d71f436d870a791f8e211 (RESTRICTIVE) tok#9, 41 bytes
  Our: could not find expected ':' at 5:0
  Input: - item1ite\n- it 3\n- truetem1\n- ite\n-null\n

[17] crash-0c915b012abd8468f15c88278bda230ec47fac1e (RESTRICTIVE) tok#3, 43 bytes
  Our: could not find expected ':' at 2:0
  Input: - ite\rm item1\n- itkey:lsl1\n- itkeye,valusl\n

[18] crash-1388c722e1f7303c89ab8fdcd8ad71a5fd3837de (PERMISSIVE) tok#13, 45 bytes
  Ref: found a tab character that violates indentation at 4:0
  Input: ---\ndoc1: value1\n---\ndoc2: value2\n\tme\nta%TAda

[19] crash-7fe57ec90d90b6f61ec8ed397e6d40b7e008ad92 (RESTRICTIVE) tok#9, 46 bytes
  Our: could not find expected ':' at 5:0
```

## Memory Safety

No memory safety issues found across 68503033 ASAN-instrumented executions.

## Artifacts

- Raw logs: `test-output/campaign-a6b8af10/`
- Crash artifacts: `test-output/campaign-a6b8af10/crashes-*/`
- Corpus: `test-output/campaign-a6b8af10/corpus-*/`
- Classification: `test-output/campaign-a6b8af10/classify-output.txt`
