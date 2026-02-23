# Differential Fuzzing Verification — cfe98d4a

**Date:** 2026-02-23T16:21Z (updated 2026-02-23T16:45Z with multi-level analysis)
**Commit:** cfe98d4a (includes flow_level guard fix 99ca2cbf)
**Purpose:** Verify worker claim of zero divergences after recent fixes
**Result:** **3 TRUE DIVERGENCES at loader level; 5 both-fail cosmetic; 6 match**

## CORRECTION NOTICE

The original version of this report used a scanner-only classifier that over-reported divergences as "PERMISSIVE" or "RESTRICTIVE" based on token-count differences. A follow-up multi-level analysis (scan + parse + load) revealed that most scanner-level divergences are cosmetic (both libraries ultimately reject the input). The true divergences are at the **loader level** where our loader swallows parse errors.

## Campaign Parameters

| Parameter | Value |
|-----------|-------|
| Duration | 10 minutes allocated, divergences found in 4 seconds |
| Executions | 154 |
| Fork mode | 3 workers |
| Corpus | 18 inputs |
| Coverage edges | 1399 |
| Seed sources | yaml-test-suite, yaml-test-data, yaml-fuzz (706 seeds) |
| Max input length | 8192 bytes |

## Multi-Level Analysis (14 crash files)

| Verdict | Count | Description |
|---------|-------|-------------|
| TRUE DIVERGENCE | 3 | Our loader returns SUCCESS, reference returns ERROR |
| BOTH-FAIL | 5 | Both libraries reject at all levels (cosmetic token-count differences) |
| MATCH | 6 | Identical behavior or only internal cosmetic differences |

## 3 True Divergences (all at loader level)

All 3 share the same pattern: both libraries fail at the parser level with **identical** errors, but our loader returns SUCCESS (2 docs) while the reference loader correctly propagates the parse error.

### Divergence #1 (30 bytes)
- **Input:** `key: [ word1\n#  xxx\n  word2 ]\n`
- **Scan:** BOTH SUCCESS (11 tokens each)
- **Parse:** BOTH ERROR "did not find expected ',' or ']'" @2:2
- **Load:** **OUR=SUCCESS (2 docs)** vs **REF=ERROR**

### Divergence #2 (18 bytes) — simplest reproducer
- **Input:** `---\n[ , a, b, c ]\n`
- **Scan:** BOTH SUCCESS (11 tokens each)
- **Parse:** BOTH ERROR "did not find expected node content" @1:2
- **Load:** **OUR=SUCCESS (2 docs)** vs **REF=ERROR**

### Divergence #3 (37 bytes)
- **Input:** `{ &a [a, &b bb*: ], *a : [ ,c*b, d]}\n`
- **Scan:** BOTH SUCCESS (23 tokens each)
- **Parse:** BOTH ERROR "did not find expected node content" @0:27
- **Load:** **OUR=SUCCESS (2 docs)** vs **REF=ERROR**

**Root cause hypothesis:** The loader catches the parse error during document loading but treats it as end-of-stream (returning an empty document) rather than propagating the error. The "2 docs" count means it loaded one document successfully before the error, then returned an empty document signaling stream end — instead of returning 0 (failure).

## 5 Both-Fail Cases (cosmetic, not true divergences)

These inputs are rejected by BOTH libraries at all API levels. The differences are:
- Different token counts before the error (one library emits 1-2 more tokens)
- Different error messages in 2 cases ("found character that cannot start any token" vs "found a tab character that violates indentation")
- Same final outcome: ERROR at scan, parse, and load levels

| Input | Our scan error | Ref scan error |
|-------|---------------|----------------|
| `foo: 1\n\t\nbar: 2\n` | "cannot start any token" @1:0 | "tab violates indentation" @1:0 |
| `foo:\n  a: 1\n  \tb: 2\n` | "cannot start any token" @2:2 | "tab violates indentation" @2:2 |
| `- [ a, b ]\n- C a: b }\na"...` | "could not find expected ':'" @3:0 | same @3:0 |
| `a:\n  b:\n   +#c: d\n  e>...` | "could not find expected ':'" @4:2 | same @4:2 |
| `foo:\n  bar\ninvalid\n` | "could not find expected ':'" @3:0 | same @3:0 |

Note: The 2 tab cases have different error **messages** (error divergence) but the same error **outcome** (both reject). These are low-severity cosmetic differences, not user-visible behavior changes.

## Memory Safety

No ASAN errors in 154 executions. Single-library harnesses (fuzz_scan, fuzz_parse, fuzz_load) ran clean in prior 10-minute campaign (13.7M executions, 0 crashes).

## Conclusion

The true divergence count is **3**, not 8 as originally reported. All 3 are loader-level bugs where our loader swallows parse errors and returns success. The remaining 5 scanner-level differences are cosmetic (both libraries reject). The loader error-swallowing bug is the sole remaining correctness issue blocking the zero-divergence milestone.
