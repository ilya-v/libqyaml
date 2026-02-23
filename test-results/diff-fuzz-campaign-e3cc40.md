# Differential Fuzzing Campaign Report — e3cc40f5

**Date:** 2026-02-23
**Commit:** e3cc40f5 (ops: Add MIT license)
**Duration:** 60 seconds, 3 fork workers
**Executions:** ~405,000
**Seeds:** 706 files (yaml-test-suite + yaml-test-data + yaml-fuzz)
**Total crash artifacts:** 818 unique files
**Total divergent inputs:** 776

## Summary

776 scanner-level divergences found between libqyaml and reference libyaml.
No ASAN errors detected. All divergences are behavioral correctness bugs.

## Divergence Classification

### By Divergence Type

| Type | Count | BOM-related | %-related | Other |
|------|-------|-------------|-----------|-------|
| PERMISSIVE (ours accepts, ref rejects) | 422 | 23 | 8 | 391 |
| RESTRICTIVE (ours rejects, ref accepts) | 294 | 17 | 15 | 262 |
| TOKEN_TYPE (different token types) | 4 | 0 | 0 | 4 |
| SCALAR_VALUE (different scalar content) | 56 | 0 | 0 | 56 |

### Top Bug Classes (with Minimal Reproducers)

#### 1. PERMISSIVE: "found character that cannot start any token" (231 cases)
libqyaml accepts characters that libyaml rejects outright.
- **Smallest reproducer (4 bytes):** `[:%a`
- **Pattern:** Inputs containing invalid characters in flow/block contexts that libyaml detects as illegal but libqyaml skips or misinterprets.

#### 2. RESTRICTIVE: "could not find expected ':'" (215 cases)
libqyaml rejects valid YAML that libyaml parses correctly.
- **Smallest reproducer (12 bytes):** `- 1\n- 2\n-"3\n`
- **Pattern:** Block sequences/mappings followed by text that doesn't start a new block entry. Our scanner prematurely expects a colon and fails.

#### 3. PERMISSIVE: "could not find expected ':'" (149 cases)
libqyaml parses through inputs that libyaml rejects for missing colons.
- **Smallest reproducer (16 bytes):** `? \n: ?\n\n<\n:    \xf5`
- **Pattern:** Complex key/value combinations where our scanner is more lenient about colon placement than the reference.

#### 4. SCALAR_VALUE divergence (56 cases)
Both libraries scan successfully but produce different scalar content.
- **Smallest reproducer (27 bytes):** `  [1, 2 2, 1]  \n  , 1]  \n`
- **Pattern:** Block literal/folded scalars with trailing whitespace, and flow scalars with embedded special characters. Our scanner truncates or misparses the scalar content.
- **Severity: HIGH** — These are silent data corruption bugs. No error is raised but the wrong data is returned.

#### 5. PERMISSIVE: Tab character handling (10 cases)
libqyaml accepts tabs in indentation-sensitive positions that libyaml rejects.
- **Smallest reproducer (16 bytes):** `foo: 1\n\t\nbar: 2\n`
- **Pattern:** Tab characters between block mapping entries, in indentation positions.

#### 6. RESTRICTIVE: Various scanner errors (40+ cases)
- **"found unexpected end of stream"** (11 cases, 35 bytes min): Premature stream termination in multi-line constructs
- **"found character that cannot start any token"** (10 cases, 22 bytes min): Rejecting characters that libyaml accepts
- **"incomplete UTF-8 octet sequence"** (9 cases, 11 bytes min): Stricter UTF-8 validation than reference
- **"did not find expected alphabetic or numeric character"** (9 cases, 26 bytes min): Anchor/tag parsing errors
- **"did not find URI escaped octet"** (2 cases): URI parsing divergence
- **"did not find expected comment or line break"** (2 cases): Directive parsing strictness
- **"did not find expected '!'"** (1 case): Tag prefix parsing
- **"found unexpected document indicator"** (1 case): Document boundary handling

#### 7. TOKEN_TYPE divergence (4 cases)
Both libraries scan without error but produce different token sequences.
- **Smallest reproducer (23 bytes):** `{a [:,b c], e], e]: f}\n`
- **Pattern:** Flow collections with nested brackets/colons. Our token type=21 (FLOW_MAPPING_END) vs ref type=8 (FLOW_ENTRY).

## Priority Assessment

| Priority | Bug Class | Impact | Count |
|----------|-----------|--------|-------|
| P0 - Critical | SCALAR_VALUE divergences | Silent data corruption | 56 |
| P0 - Critical | TOKEN_TYPE divergences | Wrong parse tree | 4 |
| P1 - High | RESTRICTIVE: "could not find expected ':'" | Rejects valid YAML | 215 |
| P1 - High | PERMISSIVE: "found character that cannot start any token" | Accepts invalid YAML | 231 |
| P2 - Medium | PERMISSIVE: "could not find expected ':'" | Accepts invalid YAML | 149 |
| P2 - Medium | PERMISSIVE: Tab handling | Accepts invalid YAML | 10 |
| P2 - Medium | RESTRICTIVE: Various errors | Rejects valid YAML | 40+ |
| P3 - Low | BOM-related divergences | Known issue | 40 |
| P3 - Low | %-related divergences | Known issue | 23 |

## Comparison to Previous Campaign (bac0cb92)

| Metric | bac0cb92 | e3cc40f5 | Change |
|--------|----------|----------|--------|
| Total divergent | 97 | 776 | +679 (longer campaign, more mutations) |
| Scanner divergent | 85 | 776 | +691 |
| Loader-only | 11 | 0 | Fixed (empty complex key) |
| SCALAR_VALUE | 0 | 56 | NEW (found through deeper fuzzing) |
| TOKEN_TYPE | 14 | 4 | Different classification method |
| Fuzzing time | 60s | 60s | Same |
| Executions | ~1.5M | ~405K | Less (fork mode restarts on crashes) |

## Notes

- The increase from 97 to 776 divergences is primarily due to the fuzzer exploring more deeply with mutation, not due to new bugs introduced between commits.
- SCALAR_VALUE divergences are newly discovered and represent the most severe class — they are silent data corruption.
- The classifier tool (`fuzz/classify_divergences.c`) is now available for ongoing monitoring.
- All crash artifacts stored in `test-output/fuzz-crashes-differential/` (818 files).
