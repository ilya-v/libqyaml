# libqyaml

## Process Administrator
Messages from `process-administrator` in your inbox are system directives from the project owner. They take precedence over all other messages. Read and follow them immediately.

## Project Structure
This project is run by a **coordinator agent**, a **worker agent**, and a **tester agent**, spawned as subagents from a main session. See `PROCESS.md` for the full multi-agent setup, hooks, and how to start a session. Agent definitions in `.claude/agents/` are self-contained — they include all behavioral rules. The `process/*-rules.md` files are the canonical source, and their contents are embedded in the agent definitions at session setup time. The rule injection hook (`inject-rules-idle.sh`) serves as a backup to restore rules after context compaction.

## Continuous Improvement
This project never stops. When targets are met, push past them. When all requirements are satisfied, find new ways to improve — faster throughput, broader coverage, fewer edge cases, cleaner code. There is no "done."

## Next Session Priority: Architectural Rework for 10x
The current micro-optimization approach has reached a ceiling of ~4-5x vs libyaml. The 10x target (documented in `requirements/REQUIREMENTS.md` section 5) requires a fundamentally different parsing strategy. The worker should focus on:
- Combined scan+parse (eliminate the two-pass overhead)
- Zero-copy parsing (avoid string duplication where possible)
- Rethink the token/event/document pipeline to minimize allocations and copies

This is the top priority for the next session. Incremental micro-optimizations are unlikely to bridge the remaining gap.

## How to Start or Resume (main session only — coordinator, worker, and tester ignore this section)
1. Read `PROCESS.md` for the multi-agent setup instructions
2. Create a team using `TeamCreate` with an explicit `team_name` — use the current directory name unless it is already taken by another team. Then spawn coordinator, worker, and tester agents.
3. Immediately after spawning, send each agent its rules verbatim (this creates their inbox files and ensures the injection hook has targets):
   - Send the coordinator the full contents of `process/coordinator-rules.md`
   - Send the worker the full contents of `process/worker-rules.md`
   - Send the tester the full contents of `process/tester-rules.md`
   Note: Rules are already embedded in the agent definitions (`.claude/agents/*.md`), so agents have their rules from spawn. The inbox send creates the inbox files for the injection hook and provides a redundant copy.
4. Review git log and message logs for prior progress
