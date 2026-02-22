#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$PROJECT_DIR/logs"
mkdir -p "$LOGS_DIR"

INPUT=$(cat)
AGENT="coordinator"
LOG_FILE="$LOGS_DIR/hook-${AGENT}.log"
TOOL=$(echo "$INPUT" | jq -r '.tool_name // "unknown"' 2>/dev/null)

# Detect team name on first call
if [ ! -f "$LOGS_DIR/team-name.txt" ]; then
  for d in ~/.claude/teams/*/config.json; do
    if jq -e --arg cwd "$PROJECT_DIR" '.members[0].cwd == $cwd' "$d" >/dev/null 2>&1; then
      jq -r '.name' "$d" > "$LOGS_DIR/team-name.txt"
      break
    fi
  done
fi

TIMESTAMP_FILE="$LOGS_DIR/inject-${AGENT}.ts"
INJECT_INTERVAL=600  # 10 minutes in seconds

NOW=$(date +%s)
LAST=$(cat "$TIMESTAMP_FILE" 2>/dev/null || echo 0)
ELAPSED=$((NOW - LAST))

if [ "$ELAPSED" -ge "$INJECT_INTERVAL" ]; then
  echo "$NOW" > "$TIMESTAMP_FILE"
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] INJECTED rules (tool=$TOOL, elapsed=${ELAPSED}s)" >> "$LOG_FILE"
  RULES=$(cat "$SCRIPT_DIR/coordinator-rules.md" 2>/dev/null)
  jq -n --arg rules "$RULES" \
    '{hookSpecificOutput: {permissionDecision: "allow", additionalContext: $rules}}'
else
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] skipped (tool=$TOOL, elapsed=${ELAPSED}s)" >> "$LOG_FILE"
  echo '{"hookSpecificOutput": {"permissionDecision": "allow"}}'
fi
