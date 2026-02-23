# libqyaml Requirements

## 1. Goal

Create an API-compatible drop-in replacement for libyaml, focused on optimization (throughput, latency), to make it as fast or faster than the best-in-class competitors.

## 2. API Compatibility

- Must be fully API-compatible with libyaml — same header, same types, same functions, same behavior
- Programs linking against libyaml must be able to switch to libqyaml by replacing the library with no source changes
- Correctness first, then performance — never sacrifice conformance for speed
- Focus on read/parse performance as the primary optimization target

## 3. Reference Implementation

- Clone and build libyaml as a reference implementation
- Use libyaml's source, tests, and behavior as the ground truth
- Reproduce libyaml's behavior exactly — match its output for all valid inputs
- Use libyaml's test suite as a baseline: libqyaml must pass all of libyaml's existing tests without modification

## 4. Testing

### 4.1 Conformance Tests
- libyaml's own test suite must pass unmodified against libqyaml
- Any YAML spec conformance tests that libyaml passes, libqyaml must also pass

### 4.2 Unit Tests
- Comprehensive unit test suite covering all public API functions
- Edge cases, error paths, and boundary conditions
- Minimum 1000 unit tests
- Code coverage must be close to 100%

### 4.3 Fuzz Testing
- Structure-aware fuzzing of the parser (not just random bytes)
- Fuzz both the event-based and document-based APIs
- Run fuzzing continuously and fix all issues found
- **Bonus: if fuzzing discovers bugs in libyaml itself, document them and ensure libqyaml avoids them**

### 4.4 Differential Testing
- Compare libqyaml output against libyaml output on the same inputs
- Any divergence is a bug in libqyaml (unless it's a documented libyaml bug)

### 4.5 OOM Injection
- Systematically fail every allocation point (malloc, realloc, calloc) and verify the library handles it gracefully — no crashes, no leaks, no undefined behavior
- Use libyaml's custom allocator hooks to inject failures without modifying library source

### 4.6 Memory Safety and Correctness
- All tests must pass under valgrind (memcheck) with zero errors
- All tests must pass under AddressSanitizer (ASAN) with zero errors
- All tests must pass under UndefinedBehaviorSanitizer (UBSAN) with zero errors
- All tests must pass under LeakSanitizer (LSAN) with zero leaks
- All tests must pass under MemorySanitizer (MSAN) with zero uninitialized reads (note: requires full-program instrumentation)
- Use a guard-page allocator (e.g., Electric Fence / DUMA) to catch any buffer overruns immediately — every 1-byte overrun must segfault, not silently corrupt
- Use an allocation-tracking allocator that records every alloc/free and verifies at teardown that every allocation was freed exactly once — catches leaks and double-frees precisely

### 4.7 Input Robustness
- Input truncation testing: feed every valid input truncated at every byte offset — the parser must return an error cleanly, never crash
- Stack depth testing: deeply nested YAML (thousands of levels) to verify recursion limits are enforced and stack overflow is handled gracefully, not crashed into
- Resource-constrained runs: test under `ulimit` restrictions (limited stack size, memory, file descriptors) to verify graceful degradation

### 4.8 Compiler Warnings and Static Analysis
- Must compile with zero warnings under both gcc and clang with `-Wall -Wextra -Wpedantic`
- Run clang `scan-build` or `cppcheck` and fix all reported issues
- Zero warnings from static analysis on the library source

### 4.9 Documentation
- All testing, benchmarking, and fuzzing results must be documented, focusing on outcomes and measurable metrics
- Document: pass/fail counts, coverage percentages, throughput numbers, crash counts, memory errors found and fixed, fuzz corpus size, and any regressions

### 4.10 Test Execution
- Tests must run in parallel, allocating up to 4 cores
- A complete testing loop (compile + run all tests) must finish within 2 minutes
- If the test suite grows beyond the 2-minute budget, split into two sets:
  - **Light test set**: used during development iteration, must stay under 2 minutes
  - **Full test set**: includes all tests, run before commits and during CI
- Development iteration always uses the light test set to keep the feedback loop fast

### 4.11 Per-Commit Validation Reports
- Every code change commit must be followed by a committed validation report covering that commit
- Each validation report must include:
  - A fast unit test subset providing overall test coverage (pass/fail counts, assertion counts, coverage percentage)
  - Benchmark results against the baseline reference library (libyaml) across the standard workload set
- The fast validation pass (compile + unit test subset + benchmarks) must complete within 30 seconds — this requires a dedicated curated subset of tests designed to fit the time budget while still providing meaningful coverage
- Benchmarks against the reference library should run at least once per validation, and may run multiple times for statistical confidence if time permits
- Validation reports are committed to `test-results/`, tagged with the commit ID they validated
- The full test suite (sanitizers, fuzzing, OOM injection, etc.) runs separately and does not need to fit in the 30-second budget

## 5. Directory Structure

### 5.1 Source Code (git-tracked)

| Directory | Contents | Owner |
|-----------|----------|-------|
| `src/` | Library implementation (C source files) | Worker |
| `include/` | Public API headers | Worker |
| `tests/` | Test source code (unit tests, integration tests, conformance tests) | Tester |
| `bench/` | Benchmark source code and workload data | Tester |
| `fuzz/` | Fuzz harness source code | Tester |

### 5.2 Build Output (gitignored)

| Directory | Contents |
|-----------|----------|
| `build/` | CMake build output (main tree) |
| `build-*/` | Variant builds (debug, release, asan, coverage, pgo, etc.) |
| `.worktree/` | Git worktree used by the tester for isolated builds |

### 5.3 Testing Artifacts

| Directory | Contents | Tracked |
|-----------|----------|---------|
| `test-output/` | Transient testing artifacts — raw logs, intermediate results, scratch data. Gitignored. May be deleted at any time without loss. | No |
| `test-results/` | Permanent testing records — per-commit validation reports, benchmark reports, coverage reports, fuzz campaign summaries, safety audit reports. All significant testing artifacts must be written here or copied here. Tagged with the commit ID they cover. | Yes |

**Rules for `test-results/`:**
- Every per-commit validation report goes here (not `test-output/`)
- Benchmark comparison reports go here
- Coverage analysis reports go here
- Fuzz campaign summaries go here (corpus stats, crashes found, time run)
- Safety audit reports (ASAN, UBSAN, Valgrind, guard-page, alloc-tracker) go here
- Files must be tagged with the 6-character commit ID they cover (e.g., `validation-a1b2c3.md`, `benchmark-a1b2c3.md`)
- This directory is the permanent record of the project's quality history

### 5.4 Other Directories

| Directory | Contents | Tracked |
|-----------|----------|---------|
| `reference/` | Reference libyaml source and pre-built library | Yes |
| `requirements/` | Project requirements (this file) | Yes |
| `process/` | Agent rules and process scripts | Yes |
| `cmake/` | CMake modules and helpers | Yes |
| `scripts/` | Utility scripts | Yes |
| `logs/` | Runtime logs (injection, project log) | No |

## 6. Performance

- Benchmark against libyaml and other best-in-class YAML parsers
- Target: at least 10x faster than libyaml on most throughput metrics
- Target: equal or better throughput and latency than the fastest competitors
- Benchmark on a variety of input types (small documents, large documents, deeply nested, many scalars, etc.)
- A major or total architecture rework is a viable path to achieving the throughput targets — do not limit optimization to incremental micro-optimizations on the existing libyaml architecture if a redesign would yield better results

## 7. Process

- Report status to the coordinator after every meaningful step — before and after making changes, after running tests, whenever hitting a blocker. Never work silently.
- Listen to the coordinator's guidance on priorities and project direction
- Install whatever tools, packages, and dependencies are needed — no restrictions on tooling
- Commit at meaningful milestones using conventional commit messages:
  - `feat:` — new functionality or capability
  - `fix:` — bug fixes
  - `test:` — test additions, changes, or infrastructure
  - `ops:` — build system, CI, tooling, process, or infrastructure changes
  - Keep the subject line short and descriptive; use the body for details when needed
