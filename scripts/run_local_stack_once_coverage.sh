#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
if [ -z "${HERMENEUTIC_ROOT:-}" ]; then
  HERMENEUTIC_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
fi
export LOCAL_STACK_SCRIPT=${LOCAL_STACK_SCRIPT:-"$HERMENEUTIC_ROOT/scripts/run_local_stack_once.sh"}
exec "$HERMENEUTIC_ROOT/scripts/run_local_stack_coverage.sh" "$@"
