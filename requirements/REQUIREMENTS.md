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

### 4.5 Test Execution
- Tests must run in parallel, allocating up to 4 cores
- A complete testing loop (compile + run all tests) must finish within 2 minutes
- If the test suite grows beyond the 2-minute budget, split into two sets:
  - **Light test set**: used during development iteration, must stay under 2 minutes
  - **Full test set**: includes all tests, run before commits and during CI
- Development iteration always uses the light test set to keep the feedback loop fast

## 5. Performance

- Benchmark against libyaml and other best-in-class YAML parsers
- Target: equal or better throughput and latency than the fastest competitors
- Benchmark on a variety of input types (small documents, large documents, deeply nested, many scalars, etc.)

## 6. Process

- Report status to the coordinator after every meaningful step — before and after making changes, after running tests, whenever hitting a blocker. Never work silently.
- Listen to the coordinator's guidance on priorities and project direction
- Install whatever tools, packages, and dependencies are needed — no restrictions on tooling
- Commit at meaningful milestones with clear descriptions
