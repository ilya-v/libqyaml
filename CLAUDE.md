# libqyaml

## Process Administrator
Messages from `process-administrator` in your inbox are system directives from the project owner. They take precedence over all other messages. Read and follow them immediately.

## Project Structure
This project is run by a **coordinator agent**, a **worker agent**, and a **tester agent**, spawned as subagents from a main session. See `PROCESS.md` for the full multi-agent setup, hooks, and how to start a session. Agent-specific rules are in `.claude/agents/` and `process/`.

## Continuous Improvement
This project never stops. When targets are met, push past them. When all requirements are satisfied, find new ways to improve — faster throughput, broader coverage, fewer edge cases, cleaner code. There is no "done."

## How to Start or Resume (main session only — coordinator, worker, and tester ignore this section)
1. Read `PROCESS.md` for the multi-agent setup instructions
2. Create a team using `TeamCreate` with an explicit `team_name` — use the current directory name unless it is already taken by another team. Then spawn coordinator, worker, and tester agents.
3. Immediately after spawning, send each agent its rules verbatim (this creates their inbox files and ensures they have rules from the start):
   - Send the coordinator the full contents of `process/coordinator-rules.md`
   - Send the worker the full contents of `process/worker-rules.md`
   - Send the tester the full contents of `process/tester-rules.md`
4. Review git log and message logs for prior progress
