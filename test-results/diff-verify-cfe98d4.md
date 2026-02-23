# Differential Fuzzing Verification — cfe98d4a

**Date:** 2026-02-23T16:21Z
**Commit:** cfe98d4a (includes flow_level guard fix 99ca2cbf)
**Purpose:** Verify worker claim of zero divergences after recent fixes
**Result:** **8 TRUE DIVERGENCES REMAIN**

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

## Divergences Found

| Category | Count |
|----------|-------|
| PERMISSIVE (libqyaml accepts, reference rejects) | 4 |
| RESTRICTIVE (libqyaml rejects, reference accepts) | 4 |
| TOKEN_TYPE | 0 |
| SCALAR_VALUE | 0 |
| ERROR_DIVERGENCE | 0 |
| **Total** | **8** |

## Detailed Divergences

### PERMISSIVE #1: Tab in indentation (16 bytes)
- **Input:** `foo: 1\n\t\nbar: 2\n`
- **Token #6:** OUR produces SCALAR "1", REF rejects with SCANNER_ERROR "found a tab character that violates indentation" at 1:0
- **Root cause:** libqyaml does not reject tab characters in indentation context

### PERMISSIVE #2: Tab in nested mapping (20 bytes)
- **Input:** `foo:\n  a: 1\n  \tb: 2\n`
- **Token #10:** OUR produces BLOCK_MAP_START, REF rejects with SCANNER_ERROR "found a tab character that violates indentation" at 2:2
- **Root cause:** Same tab handling bug

### PERMISSIVE #3: Flow collection boundary (35 bytes)
- **Input:** `- [ a, b ]\n- C a: b }\na"\n- 'b'\n- c\n`
- **Token #15:** OUR produces BLOCK_END at 2:0, REF rejects with SCANNER_ERROR "could not find expected ':'" at 3:0
- **Root cause:** After parsing mapping value `b }`, libqyaml doesn't enforce colon expectation on the next line `a"` which starts an implicit key

### PERMISSIVE #4: Mapping indentation depth (30 bytes)
- **Input:** `a:\n  b:\n   +#c: d\n  e>\n  h: i\n`
- **Token #15:** OUR produces BLOCK_END at 3:2, REF rejects with SCANNER_ERROR "could not find expected ':'" at 4:2
- **Root cause:** After nested mapping `+#c: d`, `e>` at reduced indentation triggers colon expectation in libyaml but not libqyaml

### RESTRICTIVE #5-8: Colon expectation too strict (19-31 bytes)
- **Inputs:** Various multi-line block structures followed by content at column 0
- **Pattern:** `foo:\n  bar\ninvalid\n` — libqyaml rejects "could not find expected ':'" at 3:0, libyaml accepts
- **Root cause:** libqyaml incorrectly requires a colon after what it treats as an implicit key, but the content at column 0 should end the block context, not trigger a colon expectation

## Bug Class Summary

| Bug Class | Type | Count | Severity |
|-----------|------|-------|----------|
| Tab-in-indentation | PERMISSIVE | 2 | Medium (spec violation) |
| Colon expectation too lenient | PERMISSIVE | 2 | Medium (accepts invalid YAML) |
| Colon expectation too strict | RESTRICTIVE | 4 | High (rejects valid YAML) |

## Memory Safety

No ASAN errors in 154 executions. Single-library harnesses (fuzz_scan, fuzz_parse, fuzz_load) ran clean in prior 10-minute campaign (13.7M executions, 0 crashes).

## Conclusion

The flow_level guard fix (99ca2cbf) did not eliminate all divergences. 8 true divergences remain, all related to tab handling and colon expectation logic. The RESTRICTIVE cases (rejecting valid YAML) are higher priority as they break compatibility with valid inputs that libyaml accepts. No data corruption (SCALAR_VALUE) or memory safety issues found.
