#!/usr/bin/env bash
# Bring up the compose demo for a bounded duration, then validate CSVs.

set -euo pipefail

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
RUN_DURATION=${RUN_DURATION:-15}
DOCKER_SCRIPT=${DOCKER_SCRIPT:-"$ROOT_DIR/scripts/docker_run.sh"}
OUTPUT_DIR=${OUTPUT_DIR:-"$ROOT_DIR/output"}
COMPOSE_FILE=${COMPOSE_FILE:-"$ROOT_DIR/docker/compose.yml"}

if [ ! -x "$DOCKER_SCRIPT" ]; then
  echo "[docker-stack-once] missing executable: $DOCKER_SCRIPT" >&2
  exit 1
fi

if ! [[ "$RUN_DURATION" =~ ^[0-9]+$ ]]; then
  echo "[docker-stack-once] RUN_DURATION must be an integer number of seconds" >&2
  exit 1
fi

if [ "$#" -eq 0 ]; then
  set -- up
fi

STACK_LOG=${STACK_LOG:-""}
CMD=("$DOCKER_SCRIPT" "$@")

if [ -n "$STACK_LOG" ]; then
  echo "[docker-stack-once] streaming compose logs to $STACK_LOG"
  : > "$STACK_LOG"
  ("${CMD[@]}" | tee -a "$STACK_LOG") &
else
  echo "[docker-stack-once] launching ${CMD[*]}"
  "${CMD[@]}" &
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
  echo "[docker-stack-once] stopping compose after ${RUN_DURATION}s"
  signal_group INT
  if ! wait_for_exit 5; then
    echo "[docker-stack-once] compose still running, sending SIGTERM"
    signal_group TERM
    if ! wait_for_exit 5; then
      echo "[docker-stack-once] compose unresponsive, sending SIGKILL"
      signal_group KILL
    fi
  fi
fi
wait "$stack_pid" >/dev/null 2>&1 || true

echo "[docker-stack-once] running docker compose down"
docker compose -f "$COMPOSE_FILE" down >/dev/null 2>&1 || true

echo "[docker-stack-once] running CSV validation"
python3 "$ROOT_DIR/scripts/validate_csv.py" "$OUTPUT_DIR"
