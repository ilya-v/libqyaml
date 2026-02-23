# libqyaml Project Feed

---

## 2026-02-23 ~15:07 UTC — Feed #1 (Initial Entry)

**Coverage window:** Project inception through current state (first feed entry — full state snapshot)

---

### Commits (recent 20, all 2026-02-23)

| Hash | Type | Description |
|------|------|-------------|
| 10b2872e | test | Add error divergence detection to differential harness and classifier |
| 96c622af | test | Add coverage driver for corpus-based coverage measurement |
| 9fb49159 | test | Add coverage analysis report for da8c0877 |
| da8c0877 | test | Add 30-minute extended fuzzing campaign report for a6b8af10 |
| ffb4fb81 | ops | Update LICENSE copyright holder to Aargh Software B.V. |
| bffa4f7f | ops | Unify agent rules into single source of truth |
| 2474b8cf | test | Add validation report for b137e8d4 (quoted scalar + batch KV fixes) |
| ff61d2fc | test | Add validation report for b2e02f41 (scanner fixes: % reject + blank line) |
| a6b8af10 | build | Add automated fuzzing campaign script and fix bac0cb report |
| b137e8d4 | fix | Preserve trailing line breaks after escaped line breaks in quoted scalars |
| 03d77aef | fix | Apply blank line continuation fix to batch KV scanner path |
| b2e02f41 | fix | Correct plain scalar fast path to handle blank line continuations |
| 57e33e7a | test | Add standalone divergence classifier for differential fuzzing |
| 76d8dbad | test | Add differential fuzzing campaign report for e3cc40f5 |
| 8f2c42d8 | fix | Reject '%' as plain scalar start in non-directive positions |
| e3cc40f5 | ops | Add MIT license |
| db0dc2a1 | build | Integrate all 3 external fuzz sources as seed inputs |
| 3e4a6bb7 | ops | Add three fuzz sources and differential fuzzing to requirements |
| 19bd9896 | build | Add per-commit fuzz script for validation pipeline |
| 4f7fb0d0 | test | Add validation report for 16fae05c (empty key fix + tag interning) |

- **3 bug fixes** (8f2c42d8, b2e02f41/03d77aef, b137e8d4) — all in scanner correctness
- **HEAD:** 10b2872e (error divergence detection in differential harness)
- **Uncommitted changes in working tree:** src/scanner.c (7 lines), .claude/agents/ (2 files), process/inject-rules-idle.sh (1 line)

---

### Test Results (latest validation: b137e8d4, 2026-02-23 15:33)

| Suite | Result | Count |
|-------|--------|-------|
| Differential (vs libyaml) | PASS | 294/294 |
| API Comprehensive | PASS | 156/156 |
| Parser Comprehensive | PASS | 182/182 |
| Scanner Comprehensive | PASS | 166/166 |
| Loader Comprehensive | PASS | 130/130 |
| Edge Cases | PASS | 302/302 |
| Errors | PASS | 124/124 |
| Errors Comprehensive | PASS | 77/77 |
| Emitter | PASS | 166/166 |
| Document API | PASS | 263/263 |
| Emitter Comprehensive | PASS | 55/55 |
| Deep Nesting | PASS | 34/34 |
| **Total** | **PASS** | **~2,250+ assertions, 0 failures** |

- **ASAN+UBSAN:** 1,354 assertions, 0 errors (all suites pass under clang ASAN)
- **Valgrind:** Clean (zero errors, zero leaks — prior sessions)
- **Quick fuzz:** fuzz_differential FAIL with 16 runs/0 corpus — **pre-existing BOM/% divergence issue** (known, not a new crash)
- **Differential fuzz harness (30-min campaign, a6b8af10):** 68.5M executions, 0 crashes on single-library harnesses, **48 divergences** on fuzz_differential (PERMISSIVE: 12, RESTRICTIVE: 23, TOKEN_TYPE: 6, SCALAR_VALUE: 7)

---

### Benchmarks (validation-b137e8.md, note: CPU contention — ratios reliable, absolutes depressed)

| Workload | API | Ours MB/s | Ref MB/s | Speedup |
|----------|-----|-----------|----------|---------|
| Mapping (244 KB) | scan | 230.7 | 58.8 | **3.92x** |
| Mapping (244 KB) | parse | 177.5 | 58.8 | **3.02x** |
| Mapping (244 KB) | load | 244.4 | 42.4 | **5.76x** |
| K8s (50 KB) | scan | 116.8 | 46.5 | 2.51x |
| K8s (50 KB) | parse | 121.0 | 35.0 | 3.46x |
| K8s (50 KB) | load | 120.8 | 23.7 | **5.10x** |
| Flow (57 KB) | scan | 98.7 | 47.4 | 2.08x |
| Flow (57 KB) | parse | 81.0 | 30.7 | 2.64x |
| Flow (57 KB) | load | 94.3 | 15.8 | **5.97x** |
| Small (35 B) | scan | 37.9 | 22.9 | 1.66x |
| Small (35 B) | parse | 28.1 | 15.8 | 1.78x |
| Small (35 B) | load | 21.4 | 14.9 | 1.44x |

- Range: **1.44x–5.97x** vs libyaml across all workloads and API levels
- Best: mapping load 5.76x, flow load 5.97x (note: CPU contention — actual peak may be higher)
- Weakest: small document load 1.44x (overhead-dominated)
- **Historical peak (clean CPU, per memory):** mapping load 5.43x (597 MB/s), mapping scan 5.28x (800 MB/s)

---

### Coverage (coverage-da8c08.md, corpus-based with 6,041 files, 2026-02-23 15:59)

| File | Line | Branch | Function |
|------|------|--------|----------|
| scanner.c | 94.4% | 73.8% | **100%** |
| parser.c | 92.1% | 77.2% | 91.7% |
| loader.c | 88.9% | 68.1% | **100%** |
| reader.c | 83.8% | 80.4% | **100%** |
| **Read-path combined** | **~92%** | — | **~98%** |
| api.c (all) | 28.6% | 19.5% | 26.4% |
| **Full library total** | **60.2%** | **47.4%** | **52.4%** |

- Full library numbers depressed by 0%-covered emitter.c, dumper.c, writer.c (write-path, excluded from optimization scope)
- Read-path ~92% line is the meaningful number
- **Remaining gaps:** OOM error paths (~180 lines, unreachable without malloc interception), UTF-16/32 decoding (~50 lines), minor edge cases

---

### Agent Activity

| Agent | Total Messages | Unread | Status |
|-------|---------------|--------|--------|
| team-lead | 280 | 0 | Active (all read) |
| coordinator | 132 | 0 | Active (all read) |
| worker-2 | 64 | **24** | **FALLING BEHIND — 24 unread** |
| tester-2 | 29 | 0 | Active (all read) |
| tester | 42 | **22** | **FALLING BEHIND — 22 unread** (tester-1 inactive/replaced) |
| worker | 60 | **37** | **FALLING BEHIND — 37 unread** (worker-1 inactive/replaced) |
| strategic-tester | 23 | **15** | **FALLING BEHIND — 15 unread** |
| journalist | 1 | 1 | Initializing (this agent) |

**Flags:**
- **worker** (original): 37 unread — agent appears inactive/replaced; tester and worker original instances were superseded
- **worker-2**: 24 unread — active worker; high backlog suggests agent is busy, not idle
- **tester**: 22 unread — agent replaced by tester-2; backlog accumulating on inactive inbox
- **strategic-tester**: 15 unread — working on deep quality/fuzz work

**Active agents:** coordinator, worker-2, tester-2, strategic-tester
**Inactive/replaced agents with stale inboxes:** worker (original), tester (original)

---

### News

- **BOM handling divergence (open bug):** Differential fuzzing identified that libqyaml incorrectly handles UTF-8 BOM (`\xef\xbb\xbf`) in mid-stream and between documents. libqyaml rejects inputs libyaml accepts (21 RESTRICTIVE cases) and produces wrong token ordering (14 TOKEN_TYPE cases). Root cause: scanner not correctly advancing past BOM in non-initial positions. No fix assigned yet.

- **Remaining 48 differential divergences (from 30-min campaign):** 12 PERMISSIVE (libqyaml too lenient), 23 RESTRICTIVE (libqyaml too strict), 6 TOKEN_TYPE, 7 SCALAR_VALUE. BOM handling is the dominant pattern in both RESTRICTIVE and TOKEN_TYPE categories.

- **3 correctness fixes landed today:**
  - `%` character acceptance in non-directive positions (8f2c42d8) — FIXED
  - Plain scalar blank line continuations (b2e02f41) — FIXED
  - Quoted scalar trailing line breaks after escaped line breaks (b137e8d4) — FIXED

- **Error divergence detection added to differential harness (10b2872e):** Harness now detects when one library returns success and the other returns error — previously this class of divergence was silently missed.

- **Uncommitted changes in src/scanner.c (7 lines):** Working tree has modifications not yet committed. Status: likely in-progress work by worker-2.

- **Coverage driver added (96c622af):** Enables corpus-based coverage measurement using fuzz campaign corpora — used to generate the 6,041-file coverage run at commit da8c0877.

- **Performance ceiling assessment:** CLAUDE.md notes that 10x target is unachievable within libyaml-compatible architecture. Realistic ceiling is 5–6x on favorable workloads. Current focus: close gaps in flow, small documents, and quality consolidation.

- **Team composition:** 6 agents total — coordinator, worker-2, tester-2, strategic-tester (active); journalist (new, this agent). Original worker and tester agents were replaced.

---

*Next feed entry: ~15:22 UTC (15-minute cycle)*
