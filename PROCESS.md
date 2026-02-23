# Process: Multi-Agent Coordination with Rule Injection

## What This Is

A setup for running three Claude Code agents with different roles and behavioral rules, where:

- Each agent's rules are periodically injected into its inbox as messages from `team-lead` (for priority delivery)
- Rules are re-injected every 10 minutes to survive context compaction
- Each agent only sees its own rules, not the others'
- All injection activity is logged to `logs/idle-inject.log`

## Agents

Four Claude Code agents run in this setup:

| Agent | Role | How it starts |
|---|---|---|
| Main session | Thin launcher. Creates the team, spawns the other three agents, then stays idle. | User starts Claude Code in the project directory. |
| Coordinator | Non-technical project owner. Manages priorities, pushes for quality, never makes technical decisions. | Spawned by main session with `subagent_type: "coordinator"`. |
| Worker | Technical lead. Makes all implementation decisions, delegates all coding to subagents, handles commits and communication. Never runs tests, never writes code directly. | Spawned by main session with `subagent_type: "worker"`. |
| Tester | Testing expert. Owns all tests, fuzz harnesses, and benchmarks. Takes technical direction from worker, reports results to coordinator. | Spawned by main session with `subagent_type: "tester"`. |

## Files

### Configuration (checked into repo)

**`.claude/settings.json`** — Single hook: `TeammateIdle` triggers the injection script.

```json
{
  "hooks": {
    "TeammateIdle": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "process/inject-rules-idle.sh"
          }
        ]
      }
    ]
  }
}
```

**`process/inject-rules-idle.sh`** — The injection script. On each `TeammateIdle` event, identifies the agent from `teammate_name`, reads the corresponding rules file, and writes it as a message into the agent's inbox. Rate-limited to one injection per agent every 10 minutes. Uses `mkdir`-based locking compatible with Claude Code's internal `proper-lockfile`.

**`process/coordinator-rules.md`** — The coordinator's behavioral constraints. Read fresh from disk on each injection.

**`process/worker-rules.md`** — The worker's behavioral constraints.

**`process/tester-rules.md`** — The tester's behavioral constraints.

**`CLAUDE.md`** — Shared project instructions that all agents see. Includes the rule-injection directive and project-level instructions.

**`.claude/agents/coordinator.md`** — Coordinator agent definition. **Self-contained:** includes the full contents of `process/coordinator-rules.md` embedded directly, so the agent has its rules from the moment it spawns (no inbox delivery needed).

**`.claude/agents/worker.md`** — Worker agent definition. **Self-contained:** includes the full contents of `process/worker-rules.md`.

**`.claude/agents/tester.md`** — Tester agent definition. **Self-contained:** includes the full contents of `process/tester-rules.md`.

### Runtime artifacts (gitignored)

**`logs/idle-inject.log`** — Full log of every hook invocation: timestamps, input data, injection decisions, lock acquisition, success/failure.

**`logs/idle-inject-{teammate}.ts`** — Unix timestamp of last injection for each agent. Used for rate limiting.

**`logs/team-name.txt`** — The auto-generated team name.

**`logs/project-log.md`** — The coordinator's running project log: summaries of agent updates, decisions, and next steps.

**`test-output/`** — Testing artifacts produced by the tester: crash dumps, stack traces, coverage reports, sanitizer logs, benchmark results.

**`.worktree/`** — Git worktree used by the tester for isolated test builds and runs.

## How It Works

### Agent definitions are self-contained

Each agent's `.claude/agents/*.md` file includes its full behavioral rules embedded directly. This means agents have their rules from the moment they spawn — no inbox delivery needed. This solves the "first-turn deafness" problem: agents don't read their inbox until their first turn completes, which can take hours. With rules in the agent definition, they're loaded as part of the system prompt at spawn time.

The `process/*-rules.md` files remain the canonical source of truth. When updating rules, edit the `process/` file first, then re-embed it in the corresponding agent definition.

### Rule injection (backup for context compaction)

The `TeammateIdle` hook fires on the main agent whenever a teammate (coordinator, worker, or tester) goes idle — which happens between every turn. The hook script:

1. Reads `teammate_name` from the hook input to identify which agent went idle
2. Maps it to a rules file (`coordinator` → `coordinator-rules.md`, `worker` → `worker-rules.md`, `tester` → `tester-rules.md`)
3. Checks the rate limit — skips if last injection was less than 10 minutes ago
4. Reads the rules file from disk (always fresh, never cached)
5. Acquires a `mkdir`-based lock on the agent's inbox file (compatible with `proper-lockfile`)
6. Appends a message with `from: "team-lead"` to the agent's inbox JSON (team-lead messages are prioritized over peer messages in the agent's poll loop)
7. Releases the lock

The agent reads the message at the start of its next turn, processes the rules, and marks it as read.

### Why inbox injection instead of additionalContext

The previous approach used `additionalContext` on `PreToolUse:SendMessage` and `SubagentStart` hooks. This had problems:

- `SendMessage` recipient-based sender inference broke with 3 agents (main got false injections)
- `SubagentStart` only fires once at spawn
- `additionalContext` is not supported on `TeammateIdle` or `TaskCompleted` events

Inbox injection sidesteps all of these: the hook knows exactly which agent went idle (from `teammate_name`), writes directly to that agent's inbox file, and the agent processes it as a normal message.

### Context compaction

When an agent's context is compacted, previously injected rules are summarized and may lose detail. However, any unread rules message still sitting in the inbox survives compaction (the inbox is a separate file). The agent reads it at the start of the next turn, restoring the rules. At worst, one turn runs without full rules before the next injection.

### Locking protocol

Claude Code uses the `proper-lockfile` npm package for inbox file access. The protocol:

1. `mkdir` a directory at `{inbox}.json.lock` (atomic on POSIX)
2. `touch` the directory to set mtime (used for stale lock detection, default 10s threshold)
3. Read-modify-write the inbox JSON
4. `rm -rf` the lock directory

The injection script follows this same protocol.

### Worker delegation model

The worker does not write code directly. It reads and analyzes the codebase, plans changes in detail, then spawns subagents (via the `Task` tool with `subagent_type: "general-purpose"`) to do the actual coding. After the subagent completes, the worker reviews the result, commits the work, and notifies the tester.

This delegation model solves the worker responsiveness problem: spawning a subagent causes the worker to go idle, which triggers inbox polling. Without delegation, the worker runs long uninterrupted coding turns (30+ minutes) during which it cannot read any messages — missing coordinator instructions, tester results, and process reminders.

The worker's role is: read code, plan, delegate, review, commit, communicate. The subagents' role is: write code, run builds.

### Git workflow

The worker and tester share the `master` branch but work in separate trees to avoid conflicts:

- The **worker** works directly in the main tree, committing to `master`.
- The **tester** works in a git worktree at `.worktree/`, checked out from `master`. This isolates the tester from the worker's in-progress edits — the tester always builds against a known-good committed state.
- The worker must commit changes before requesting tests. The tester can only test committed code.
- When the worker commits, the tester updates the worktree to the latest `master` commit.
- The tester merges its test commits back to `master` using fast-forward merges (`git merge --ff-only`). Since the worker touches `src/`/`include/` and the tester touches `tests/`/`fuzz/`/`bench/`, fast-forward should almost always succeed. If not, the tester rebases first.
- `test-output/` is symlinked from the main tree into the worktree so all testing artifacts accumulate in one place.

## Prerequisites

- `jq` must be installed and on the PATH
- `process/inject-rules-idle.sh` must be executable (`chmod +x`)

## How to Start a Session

1. Start Claude Code in the project directory
2. Tell the main session: "Read PROCESS.md, create a team, spawn a coordinator, a worker, and a tester"
3. The main session spawns all three agents via `Task` tool with `subagent_type: "coordinator"`, `subagent_type: "worker"`, and `subagent_type: "tester"`
4. Agents start with their full rules already loaded (embedded in `.claude/agents/*.md`)
5. The main session sends each agent its rules via `SendMessage` — this creates the inbox files (needed for the injection hook) and provides a redundant copy
6. Agents begin communicating via `SendMessage`
7. Rule injection happens automatically every 10 minutes via the `TeammateIdle` hook (backup for context compaction)
8. The main session stays idle unless it needs to intervene

## Adapting This for Another Project

1. Copy `.claude/settings.json`, `process/inject-rules-idle.sh`, and the `process/*-rules.md` files
2. Edit the rules files for your project's needs
3. Add the rule-injection directive to your `CLAUDE.md`
4. Add agent definitions under `.claude/agents/` if needed
5. Ensure `jq` is installed and the script is executable
