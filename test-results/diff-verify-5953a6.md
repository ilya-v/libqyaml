# Differential Verification Report: 5953a6a7

**Commit:** 5953a6a7 (fix: Revert tab error message change that diverged from libyaml)
**Date:** 2026-02-23
**Campaign duration:** 10 minutes, 3 fork workers
**Seed corpus:** 2628 files (yaml-test-suite + yaml-test-data + yaml-fuzz)
**ASAN+UBSAN:** enabled throughout

## CORRECTION: Previous report (07faa3aa) was incorrect

The earlier "zero true divergences" report for commit 07faa3aa was **wrong**. That campaign used a smaller seed corpus (~25 crash artifacts total) and did not find the divergences reported here. Testing 07faa3aa against the same 1131 crash corpus found the **same 23 true divergences**. These bugs predate the tab message revert — they exist in both 07faa3aa and 5953a6a7.

## Results Summary

| Metric | Count |
|--------|-------|
| Total crash artifacts | 1131 |
| MATCH (loader agrees) | 0 |
| BOTH-FAIL (cosmetic, both reject) | 1074 |
| **TRUE DIVERGENCE** | **23** |
| ERROR DIVERGENCE (both fail, different error) | 34 |

### Scanner-level classification (for reference)
| Category | Count |
|----------|-------|
| PERMISSIVE (ref rejects first) | 540 |
| RESTRICTIVE (ours rejects first) | 523 |
| ERROR_DIVERGENCE | 7 |

Note: scanner-level classification over-counts because most scanner divergences resolve to BOTH-FAIL at the loader level.

## Bug Class 1: Document Node Content Divergence (15 cases)

**Severity: HIGH** — Both libraries load successfully but produce different document trees.

**Pattern:** When a block sequence entry contains an anchor and/or tag but no explicit scalar value (the value is implicitly empty), the batch loader misinterprets the structure. Instead of creating a scalar node with the anchor/tag, it appears to absorb the next sequence entry.

**Absolute minimal reproducer:** `- !a\n-\n` (7 bytes)
- libqyaml: sequence(1 item) → sequence(tag=!a, 1 item) → scalar("") — WRONG (tag applied to sequence, not scalar)
- reference: sequence(2 items) → scalar(tag=!a, ""), scalar("") — CORRECT

**Reproducer 2:** `- &a !\n- &d d\n` (14 bytes)
- libqyaml: sequence(1 item) → sequence(1 item) → scalar("d") — WRONG
- reference: sequence(2 items) → scalar(""), scalar("d") — CORRECT

**Reproducer 3:** `- !!str\n- !!null : a\n  b: !!str\n- !!str : !!null\n` (49 bytes)
- libqyaml: sequence(1 item) → nested sequence tagged `!!str` — WRONG
- reference: sequence(3 items) → scalar(""), mapping, mapping — CORRECT

**Root cause hypothesis:** The batch loader's block sequence fast path fails to emit an empty scalar node when it encounters ANCHOR/TAG tokens followed immediately by another BLOCK_ENTRY. The token streams from both libraries are **identical** — the bug is purely in the loader.

**All content divergence inputs involve:**
- Block sequences with `- &anchor`, `- !!tag`, or `- &anchor !tag` entries
- Empty implicit scalar values (value implied by next BLOCK_ENTRY or BLOCK_END)
- The batch loader incorrectly treats the anchor/tag as belonging to the sequence rather than a scalar

## Bug Class 2: Load Status Divergence (8 cases)

**Severity: MEDIUM** — libqyaml loads successfully, reference libyaml rejects with error.

**Pattern:** All 8 cases involve `[?]` or similar flow sequence constructs containing a complex key indicator. libqyaml accepts and loads these; libyaml rejects with "did not find expected ',' or ']'".

**Absolute minimal reproducer:** `[?]` (3 bytes)
- libqyaml: loads as sequence containing mapping `{: }` — WRONG
- reference: rejects with "did not find expected ',' or ']'" — CORRECT

**Reproducer 2:** `--sequeenesc-:\n- - - [?]\n\n- - - -- -[]\n- - -{}\n` (47 bytes)
- libqyaml: loads successfully with `[?]` becoming `[{: }]`
- reference: rejects at line 3 with "did not find expected ',' or ']'"

**Root cause hypothesis:** The batch flow scanner allows `?` inside flow sequences when it shouldn't. The `fetch_flow_entry_scalar` or batch scanner fast path is too permissive about what constitutes a valid flow entry.

## Error Divergences (34 cases)

Both libraries reject the input, but with different error codes or messages. These are cosmetic — not functional bugs. Common patterns:
- "could not find expected ':'" (ours) vs "did not find expected '-' indicator" (ref)
- "could not find expected ':'" (ours) vs "did not find expected key" (ref)
- "found character that cannot start any token" (ours) vs "found a tab character that violates indentation" (ref) — 2 cases remaining after the tab message revert
- "could not find expected ':'" (ours) vs "found undefined alias" (ref)

## Verification Method

1. Ran 10-minute libFuzzer differential campaign (fork=3, ASAN+UBSAN)
2. Collected 1131 crash artifacts
3. Ran standalone loader-level checker on all artifacts (`check_loader_divergence.c`)
4. Used `doc_dump.c` to compare document trees
5. Used `token_dump.c` to verify token stream agreement
6. Cross-verified against 07faa3aa build — same 23 divergences present

## Files

- Crash artifacts: `test-output/diff-verify-5953a6/crashes/` (1131 files)
- Representative reproducers: `test-output/diff-verify-5953a6/reproducers/` (5 files)
- Minimized reproducers: `test-output/diff-verify-5953a6/minimized/` (2 files)
  - `bug1-tag-bleeds-sequence.yaml` — 7 bytes: `- !a\n-\n`
  - `bug2-flow-complex-key.yaml` — 3 bytes: `[?]`
- Loader checker: `fuzz/check_loader_divergence.c`
- Document dump tool: `fuzz/doc_dump.c`
