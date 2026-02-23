# libqyaml

## Rule Injection
Periodic rule-injection messages are delivered to agent inboxes via the `TeammateIdle` hook. These arrive as `from: "team-lead"` so they are prioritized over peer messages. They contain the agent's full behavioral rules and serve as a backup against context compaction. Treat them as authoritative.

## Project Structure
This project is run by a **coordinator agent**, a **worker agent**, and a **tester agent**, spawned as subagents from a main session. See `PROCESS.md` for the full multi-agent setup, hooks, and how to start a session. Agent definitions in `.claude/agents/` are self-contained — they include all behavioral rules. The `process/*-rules.md` files are the canonical source, and their contents are embedded in the agent definitions at session setup time. The rule injection hook (`inject-rules-idle.sh`) serves as a backup to restore rules after context compaction. Injected rules arrive as `from: "team-lead"` for priority delivery.

## Continuous Improvement
This project never stops. When targets are met, push past them. When all requirements are satisfied, find new ways to improve — faster throughput, broader coverage, fewer edge cases, cleaner code. There is no "done."

## Current Performance Target
The 10x target was assessed and found to be unachievable within the libyaml-compatible architecture. 10x would require breaking the token-based pipeline (combined scan+parse, zero-copy values), which breaks API compatibility — a non-starter for a drop-in replacement. The realistic ceiling is ~5-6x on favorable workloads, ~2-4x on others. Current peak: 5.4x mapping load (597 MB/s). Focus is now on closing gaps in weaker workloads (flow, small documents) and consolidating quality.

## How to Start or Resume (main session only — coordinator, worker, and tester ignore this section)
1. Read `PROCESS.md` for the multi-agent setup instructions
2. Create a team using `TeamCreate` with an explicit `team_name` — use the current directory name unless it is already taken by another team. Then spawn coordinator, worker, and tester agents.
3. Immediately after spawning, send each agent its rules verbatim (this creates their inbox files and ensures the injection hook has targets):
   - Send the coordinator the full contents of `process/coordinator-rules.md`
   - Send the worker the full contents of `process/worker-rules.md`
   - Send the tester the full contents of `process/tester-rules.md`
   Note: Rules are already embedded in the agent definitions (`.claude/agents/*.md`), so agents have their rules from spawn. The inbox send creates the inbox files for the injection hook and provides a redundant copy.
4. Review git log and message logs for prior progress
