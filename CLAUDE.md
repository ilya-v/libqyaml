# libqyaml

## Project Structure
This project is run by a **coordinator agent** and a **worker agent**, spawned as subagents from a main session. See `PROCESS.md` for the full multi-agent setup, hooks, and how to start a session. Agent-specific rules are in `.claude/agents/` and `process/`.

## How to Resume
1. Read `PROCESS.md` for the multi-agent setup instructions
2. Create a team and spawn coordinator and worker agents
3. Review git log and message logs for prior progress
