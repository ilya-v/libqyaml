#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$PROJECT_DIR/logs"
mkdir -p "$LOGS_DIR"

INPUT=$(cat)
AGENT="worker"
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

COUNTER_FILE="$LOGS_DIR/inject-${AGENT}.count"
INJECT_EVERY=100

COUNT=$(cat "$COUNTER_FILE" 2>/dev/null || echo 0)
COUNT=$((COUNT + 1))
echo "$COUNT" > "$COUNTER_FILE"

if [ $((COUNT % INJECT_EVERY)) -eq 0 ]; then
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] INJECTED rules (tool=$TOOL, count=$COUNT)" >> "$LOG_FILE"
  RULES=$(cat "$SCRIPT_DIR/worker-rules.md" 2>/dev/null)
  jq -n --arg rules "$RULES" \
    '{hookSpecificOutput: {permissionDecision: "allow", additionalContext: $rules}}'
else
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] skipped (tool=$TOOL, count=$COUNT)" >> "$LOG_FILE"
  echo '{"hookSpecificOutput": {"permissionDecision": "allow"}}'
fi
