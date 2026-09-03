#!/usr/bin/env bash
# PostToolUse hook (Edit|Write): after editing a .cpp/.h file, runs the repo's
# banned-keyword check (scripts/test_banned_keywords.sh) and, if it fails,
# feeds its output back to Claude as additionalContext. Non-blocking/informational.

f=$(jq -r '.tool_input.file_path // empty')

case "$f" in
  *.cpp|*.h)
    repo_root=$(git rev-parse --show-toplevel 2>/dev/null) || exit 0
    OUT=$(bash "${repo_root}/scripts/test_banned_keywords.sh" 2>&1)
    if [ $? -ne 0 ]; then
      jq -n --arg ctx "$OUT" '{hookSpecificOutput:{hookEventName:"PostToolUse",additionalContext:$ctx}}'
    fi
    ;;
esac
