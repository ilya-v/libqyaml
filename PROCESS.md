# Process: Multi-Agent Coordination with Hooks

## What This Is

A setup for running two Claude Code agents with different roles and behavioral rules, where:

- Each agent's rules are periodically re-injected into its context (preventing rule drift over long conversations)
- Inter-agent messages are available in team inbox files with sender labels and timestamps
- Each agent only sees its own rules, not the other's

## Agents

Three Claude Code agents run in this setup:

| Agent | Role | How it starts |
|---|---|---|
| Main session | Thin launcher. Creates the team, spawns the other two agents, then stays idle. | User starts Claude Code in the project directory. |
| Coordinator | Non-technical project owner. Manages priorities, pushes for quality, never makes technical decisions. | Spawned by main session with `subagent_type: "coordinator"`. |
| Worker | Technical expert. Makes all implementation decisions, reports honestly to coordinator. | Spawned by main session with `subagent_type: "worker"`. |

The main session exists because agent definitions in `.claude/agents/` (and their hooks) only activate on spawned subagents — the main session cannot adopt an agent definition.

## Files

### Files you create

These files define the system. All are checked into the repo.

**`.claude/settings.json`** — Empty. No shared hooks needed. All hook logic is in the agent-specific inject scripts.

```json
{
}
```

**`.claude/agents/coordinator.md`** — Coordinator agent definition. The YAML frontmatter hooks `PreToolUse` on all standard tools (Read, Bash, Edit, Write, Glob, Grep, Task, WebFetch, WebSearch). `SendMessage` is not hookable, so we hook everything else. The body contains the coordinator's base instructions.

**`.claude/agents/worker.md`** — Worker agent definition. Same hook structure, but runs `process/inject-worker-rules.sh`.

**`process/inject-coordinator-rules.sh`** — Time-based injection script. On every hooked tool call, checks if 10 minutes have elapsed since the last injection. If so, reads `coordinator-rules.md` and outputs `additionalContext` JSON. Otherwise outputs a minimal allow-only response. Uses a timestamp file in `/tmp` for tracking.

**`process/inject-worker-rules.sh`** — Counter-based injection script. Increments a counter on every hooked tool call. Every 100 calls, reads `worker-rules.md` and injects it. Uses a counter file in `/tmp` for tracking.

**`process/coordinator-rules.md`** — The coordinator's behavioral constraints, written for repeated injection. Separate from the agent definition so it can be edited independently.

**`process/worker-rules.md`** — The worker's behavioral constraints.

**`CLAUDE.md`** — Shared project facts. All three agents see this. No agent-specific behavioral rules.

### Files generated at runtime

All session artifacts go in the `logs/` directory (gitignored).

**`logs/team-name.txt`** — The auto-generated team name (e.g., "joyful-crafting-frost"). Detected on first hook call by matching `cwd` in `~/.claude/teams/*/config.json`.

**`logs/inject-coordinator.ts`** — Timestamp of last rule injection for the coordinator.

**`logs/inject-worker.count`** — Tool call counter for the worker.

**`~/.claude/teams/{team-name}/inboxes/*.json`** — Message history for all agents. Created by Claude Code's team messaging system (not by hooks). See "Message logging" under "How It Works" for how to read these.

## How It Works

### Rule injection

`SendMessage` is not a hookable tool in Claude Code. Instead, the agent definitions hook `PreToolUse` on all standard tools (Read, Bash, Edit, Write, Glob, Grep, Task, WebFetch, WebSearch). Whenever an agent uses any tool, its inject script runs and decides whether to inject rules based on:

- **Coordinator**: Time-based. Injects rules on the first tool call, then every 10 minutes. Uses a timestamp file in `/tmp` to track the last injection time.
- **Worker**: Counter-based. Injects rules every 100 tool calls. Uses a counter file in `/tmp` to track the count.

When injecting, the script reads the rules file and outputs `additionalContext` JSON. When not injecting, it outputs a minimal allow-only JSON. Both cases return `permissionDecision: "allow"` so the tool call proceeds normally.

The rules injection uses Claude Code's `additionalContext` mechanism: when a `PreToolUse` hook outputs JSON with `hookSpecificOutput.additionalContext` on exit 0, that text is added to the agent's context without blocking the tool call.

### Message logging

The inter-agent message history is available in the team inbox files at `~/.claude/teams/{team-name}/inboxes/{agent-name}.json`. Each message has `from`, `text`, `summary`, and `timestamp` fields. To view a merged chronological log:

```bash
# Find the team directory (match by cwd)
for d in ~/.claude/teams/*/config.json; do
  if jq -e '.members[0].cwd == "/path/to/project"' "$d" >/dev/null 2>&1; then
    TEAM_DIR=$(dirname "$d")
    break
  fi
done

# Merge all inboxes chronologically
jq -s 'add | sort_by(.timestamp) | .[] | "[" + .timestamp + "] [" + .from + " → " + (.to // "?") + "] " + .text' "$TEAM_DIR"/inboxes/*.json
```

## Prerequisites

- `jq` must be installed and on the PATH
- The inject scripts must be executable (`chmod +x process/inject-*.sh`)

## How to Start a Session

1. Start Claude Code in the project directory
2. Tell the main session: "Read PROCESS.md, create a team called json-lib, spawn a coordinator and a worker"
3. The main session spawns both agents via `Task` tool using `subagent_type: "coordinator"` and `subagent_type: "worker"`
4. Each agent's hook starts automatically — rule injection is periodic, not per-message
5. The main session stays idle — the coordinator and worker communicate directly via `SendMessage`

## Adapting This for Another Project

Ensure `jq` is installed. Then create the following directory structure and files:

```
.claude/
  settings.json
  agents/
    coordinator.md
    worker.md
process/
  coordinator-rules.md
  worker-rules.md
  inject-coordinator-rules.sh
  inject-worker-rules.sh
CLAUDE.md
```

### `.claude/settings.json`

Empty — no shared hooks needed:

```json
{
}
```

### `process/inject-coordinator-rules.sh`

Time-based rule injection. Injects rules on first call, then every 10 minutes. Must be executable (`chmod +x`). Use exactly as shown:

```bash
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$PROJECT_DIR/logs"
mkdir -p "$LOGS_DIR"

INPUT=$(cat)

# Detect team name on first call
if [ ! -f "$LOGS_DIR/team-name.txt" ]; then
  for d in ~/.claude/teams/*/config.json; do
    if jq -e --arg cwd "$PROJECT_DIR" '.members[0].cwd == $cwd' "$d" >/dev/null 2>&1; then
      jq -r '.name' "$d" > "$LOGS_DIR/team-name.txt"
      break
    fi
  done
fi

AGENT="coordinator"
TIMESTAMP_FILE="$LOGS_DIR/inject-${AGENT}.ts"
INJECT_INTERVAL=600  # 10 minutes in seconds

NOW=$(date +%s)
LAST=$(cat "$TIMESTAMP_FILE" 2>/dev/null || echo 0)
ELAPSED=$((NOW - LAST))

if [ "$ELAPSED" -ge "$INJECT_INTERVAL" ]; then
  echo "$NOW" > "$TIMESTAMP_FILE"
  RULES=$(cat "$SCRIPT_DIR/coordinator-rules.md" 2>/dev/null)
  jq -n --arg rules "$RULES" \
    '{hookSpecificOutput: {permissionDecision: "allow", additionalContext: $rules}}'
else
  echo '{"hookSpecificOutput": {"permissionDecision": "allow"}}'
fi
```

### `process/inject-worker-rules.sh`

Counter-based rule injection. Injects rules every 100 tool calls. Must be executable (`chmod +x`). Use exactly as shown:

```bash
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$PROJECT_DIR/logs"
mkdir -p "$LOGS_DIR"

INPUT=$(cat)

# Detect team name on first call
if [ ! -f "$LOGS_DIR/team-name.txt" ]; then
  for d in ~/.claude/teams/*/config.json; do
    if jq -e --arg cwd "$PROJECT_DIR" '.members[0].cwd == $cwd' "$d" >/dev/null 2>&1; then
      jq -r '.name' "$d" > "$LOGS_DIR/team-name.txt"
      break
    fi
  done
fi

AGENT="worker"
COUNTER_FILE="$LOGS_DIR/inject-${AGENT}.count"
INJECT_EVERY=100

COUNT=$(cat "$COUNTER_FILE" 2>/dev/null || echo 0)
COUNT=$((COUNT + 1))
echo "$COUNT" > "$COUNTER_FILE"

if [ $((COUNT % INJECT_EVERY)) -eq 0 ]; then
  RULES=$(cat "$SCRIPT_DIR/worker-rules.md" 2>/dev/null)
  jq -n --arg rules "$RULES" \
    '{hookSpecificOutput: {permissionDecision: "allow", additionalContext: $rules}}'
else
  echo '{"hookSpecificOutput": {"permissionDecision": "allow"}}'
fi
```

### `.claude/agents/coordinator.md`

Agent definition for the coordinator. The YAML frontmatter hooks `PreToolUse` on all standard tools — since `SendMessage` is not hookable, we hook everything else and let the inject script control timing. Write your own project-specific instructions in the body:

```yaml
---
name: coordinator
description: <your coordinator description>
hooks:
  PreToolUse:
    - matcher: "Read"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Bash"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Edit"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Write"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Glob"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Grep"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "Task"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "WebFetch"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
    - matcher: "WebSearch"
      hooks:
        - type: command
          command: "process/inject-coordinator-rules.sh"
---

<your coordinator instructions here>
```

### `.claude/agents/worker.md`

Agent definition for the worker. Same hook structure:

```yaml
---
name: worker
description: <your worker description>
hooks:
  PreToolUse:
    - matcher: "Read"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Bash"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Edit"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Write"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Glob"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Grep"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "Task"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "WebFetch"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
    - matcher: "WebSearch"
      hooks:
        - type: command
          command: "process/inject-worker-rules.sh"
---

<your worker instructions here>
```

### `process/coordinator-rules.md`

The coordinator's behavioral constraints. Periodically injected into the coordinator's context (every 10 minutes). Use exactly as shown:

```markdown
# Coordinator Rules — STRICT

You are a non-technical project owner. Review these rules BEFORE sending every message.

## You MUST:
- When messaging the worker, communicate only as a non-technical project owner doing project management, requirements management and scope control
- Guide the worker and decide which phase of the project the worker should focus on
- Let the worker identify which parts are weak, why they are weak, and how to fix them
- Push for quality by asking questions and rejecting unsatisfactory answers, but never in quantitative terms

## When messaging the WORKER — you MUST NOT:
- Tell your workers how to code or test
- Name specific files (e.g., "json_read.c", "README.md", "Makefile")
- Cite specific numbers (e.g., "323 tests", "2x faster", "500 MB/s")
- Suggest algorithms or techniques (e.g., "SIMD", "Eisel-Lemire", "lookup table")
- Specify thresholds (e.g., "increase warmup to 10", "minimum 1 second")
- Compare magnitudes (e.g., "3x slower than yyjson")
- Prescribe solutions or fixes (e.g., "replace assert with FAIL", "use flock")
- Read or write any files except CLAUDE.md
- Code, do complex math, or inspect directory contents

## When messaging the MAIN (team-lead) — no restrictions:
- You may relay exact numbers, file names, technical details, and anything else the worker reported to you
- Be as detailed and specific as needed — the main session needs full visibility into the project state

## You MAY:
- Suggest high-level strategies: unit testing, fuzz testing, benchmarking, static analysis, documentation, etc.
- Reject the worker's results and ask for better quality
- Request self-evaluations and audits
- Ask the worker to explain their approach or justify their decisions
- Prioritize the worker's own identified weaknesses

## Self-check before every message to the worker:
Would a non-technical CEO say this? If not, rewrite it.
```

### `process/worker-rules.md`

The worker's behavioral constraints. Periodically injected into the worker's context (every 100 tool calls). Use exactly as shown:

```markdown
# Worker Rules

You are the technical expert responsible for all implementation decisions.

## Project Requirements
The detailed project requirements are in `requirements/REQUIREMENTS.md`. Read this file to understand the full interface specification. Only you have access to this file — the coordinator does not read it and relies on your description of the requirements.

## You MUST:
- Read `requirements/REQUIREMENTS.md` and implement the library to match it exactly
- Make all technical decisions — architecture, algorithms, optimizations, testing strategy, code quality
- Be honest with the coordinator about weaknesses and trade-offs
- Report status to the coordinator after every meaningful step — before and after making changes, after running tests, and whenever you hit a blocker. Never work silently.
- Commit at meaningful milestones with clear descriptions
- Re-validate correctness after every change (tests, conformance, memory safety)
- Follow the coordinator guidance on the project direction and iteration goals

## You MUST NOT:
- Add functionality beyond what the requirements dictate
- Wait for the coordinator to identify technical problems — find them yourself
- Hide weaknesses or overstate quality

## You MAY:
- Install any tools or dependencies you need
- Choose any algorithm, data structure, or optimization technique
- Restructure code as you see fit
- Set up any testing or benchmarking infrastructure
```

### `CLAUDE.md`

Shared project facts that all agents see. Do not put agent-specific behavioral rules here — those belong in the `process/*-rules.md` files.
