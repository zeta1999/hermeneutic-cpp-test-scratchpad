#!/usr/bin/env bash
# Build and run the local stack for a bounded duration, then validate CSV output.

set -euo pipefail

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
LOCAL_STACK_SCRIPT=${LOCAL_STACK_SCRIPT:-"$ROOT_DIR/scripts/run_local_stack.sh"}
RUN_DURATION=${RUN_DURATION:-15}
OUTPUT_DIR=${OUTPUT_DIR:-"$ROOT_DIR/output"}

if [ ! -x "$LOCAL_STACK_SCRIPT" ]; then
  echo "[local-stack-once] missing executable: $LOCAL_STACK_SCRIPT" >&2
  exit 1
fi

if ! [[ "$RUN_DURATION" =~ ^[0-9]+$ ]]; then
  echo "[local-stack-once] RUN_DURATION must be an integer number of seconds" >&2
  exit 1
fi

STACK_LOG=${STACK_LOG:-""}
if [ -n "$STACK_LOG" ]; then
  echo "[local-stack-once] streaming local stack logs to $STACK_LOG"
  : > "$STACK_LOG"
  ("$LOCAL_STACK_SCRIPT" | tee -a "$STACK_LOG") &
else
  echo "[local-stack-once] launching $LOCAL_STACK_SCRIPT"
  "$LOCAL_STACK_SCRIPT" &
fi
stack_pid=$!
sleep "$RUN_DURATION" &
sleeper=$!
wait "$sleeper" || true

signal_group() {
  local sig=$1
  kill -"$sig" "$stack_pid" >/dev/null 2>&1 || true
}

wait_for_exit() {
  local timeout=$1
  local waited=0
  while kill -0 "$stack_pid" >/dev/null 2>&1; do
    if [ "$waited" -ge "$timeout" ]; then
      return 1
    fi
    sleep 1
    waited=$((waited + 1))
  done
  return 0
}

if kill -0 "$stack_pid" >/dev/null 2>&1; then
  echo "[local-stack-once] stopping stack after ${RUN_DURATION}s"
  signal_group INT
  if ! wait_for_exit 5; then
    echo "[local-stack-once] stack still running, sending SIGTERM"
    signal_group TERM
    if ! wait_for_exit 5; then
      echo "[local-stack-once] stack unresponsive, sending SIGKILL"
      signal_group KILL
    fi
  fi
fi
wait "$stack_pid" >/dev/null 2>&1 || true

echo "[local-stack-once] running CSV validation"
python3 "$ROOT_DIR/scripts/validate_csv.py" "$OUTPUT_DIR"
