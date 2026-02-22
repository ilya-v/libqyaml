#!/bin/bash
# Full spy hook: captures complete context for ALL hook events
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DIAG_DIR="$PROJECT_DIR/logs/hook-diag"
mkdir -p "$DIAG_DIR"

INPUT=$(cat)
TS=$(date +%s%N)
EVENT=$(echo "$INPUT" | jq -r '.hook_event_name // "unknown"' 2>/dev/null)
TOOL=$(echo "$INPUT" | jq -r '.tool_name // "none"' 2>/dev/null)

{
  echo "=== PROCESS ==="
  echo "PID=$$"
  echo "PPID=$PPID"
  echo "SHELL=$SHELL"
  echo "BASH_PID=$BASHPID"
  echo "SHLVL=$SHLVL"
  echo "=== /proc/self ==="
  cat /proc/self/status 2>/dev/null | grep -E '^(Pid|PPid|Tgid|NSpid|Threads)'
  echo "=== /proc/\$PPID/cmdline ==="
  tr '\0' ' ' < /proc/$PPID/cmdline 2>/dev/null
  echo
  echo "=== /proc/\$PPID/status ==="
  cat /proc/$PPID/status 2>/dev/null | grep -E '^(Pid|PPid|Tgid|Name|Threads)'
  echo "=== ENV (full) ==="
  env | sort
  echo "=== STDIN (full JSON) ==="
  echo "$INPUT" | jq . 2>/dev/null || echo "$INPUT"
} > "$DIAG_DIR/${EVENT}_${TOOL}_${TS}_${RANDOM}.txt"

# Allow the tool call
echo '{"hookSpecificOutput": {"permissionDecision": "allow"}}'
