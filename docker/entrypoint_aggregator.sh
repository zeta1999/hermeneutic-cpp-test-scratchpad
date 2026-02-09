#!/bin/sh
set -euo pipefail

wait_for_host() {
  host="$1"
  until getent hosts "$host" >/dev/null 2>&1; do
    echo "[entrypoint] waiting for $host to resolve..."
    sleep 1
  done
}

wait_for_host "cex-notbinance"
wait_for_host "cex-notcoinbase"
wait_for_host "cex-notkraken"

echo "[entrypoint] starting aggregator service"
exec /usr/local/bin/aggregator_service "$@"
