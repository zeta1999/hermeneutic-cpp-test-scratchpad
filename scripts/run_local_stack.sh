#!/usr/bin/env bash
# Launch the same topology as docker compose, but using locally built binaries.
# Builds the needed service targets (if missing) and streams logs to the
# current terminal. Stop with Ctrl-C. Export HERMENEUTIC_WAIT_FOR_FEEDS=0 if you
# want the aggregator to skip hostname waits.

set -euo pipefail

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build"}
BUILD_TYPE=${BUILD_TYPE:-"RelWithDebInfo"}
PARALLEL=${BUILD_PARALLEL:-""}
CONFIG_PATH=${CONFIG_PATH:-"$ROOT_DIR/config/aggregator.json"}
SYMBOL=${SYMBOL:-"BTCUSDT"}
AGG_ENDPOINT=${AGG_ENDPOINT:-"127.0.0.1:50051"}
AGG_TOKEN=${AGG_TOKEN:-"agg-local-token"}
OUTPUT_DIR=${OUTPUT_DIR:-"$ROOT_DIR/output"}

if [ ! -d "$OUTPUT_DIR" ]; then
  mkdir -p "$OUTPUT_DIR"
fi
mkdir -p "$OUTPUT_DIR/bbo" "$OUTPUT_DIR/volume_bands" "$OUTPUT_DIR/price_bands"

if [ ! -f "$CONFIG_PATH" ]; then
  echo "[local-stack] missing config file: $CONFIG_PATH" >&2
  exit 1
fi

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "[local-stack] configuring CMake build directory at $BUILD_DIR"
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

TARGETS=(
  cex_type1_service
  aggregator_service
  bbo_service
  volume_bands_service
  price_bands_service
)

build_cmd=(cmake --build "$BUILD_DIR" --target "${TARGETS[@]}")
if [ -n "$PARALLEL" ]; then
  build_cmd+=(--parallel "$PARALLEL")
fi

echo "[local-stack] building service targets (${TARGETS[*]})"
"${build_cmd[@]}"

BIN_BASE="$BUILD_DIR/services"
cex_bin="$BIN_BASE/cex_type1_service/cex_type1_service"
agg_bin="$BIN_BASE/aggregator_service/aggregator_service"
bbo_bin="$BIN_BASE/bbo_service/bbo_service"
volume_bin="$BIN_BASE/volume_bands_service/volume_bands_service"
price_bin="$BIN_BASE/price_bands_service/price_bands_service"

for binary in "$cex_bin" "$agg_bin" "$bbo_bin" "$volume_bin" "$price_bin"; do
  if [ ! -x "$binary" ]; then
    echo "[local-stack] expected binary not found: $binary" >&2
    exit 1
  fi
done

declare -a PIDS=()
declare -a LABELS=()
cleaned=0

cleanup() {
  if [ "$cleaned" -eq 1 ]; then
    return
  fi
  cleaned=1
  if [ ${#PIDS[@]} -eq 0 ]; then
    return
  fi
  echo "[local-stack] stopping services"
  for pid in "${PIDS[@]}"; do
    if kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
    fi
  done
  for pid in "${PIDS[@]}"; do
    wait "$pid" 2>/dev/null || true
  done
}

trap 'cleanup' EXIT
trap 'echo "[local-stack] interrupt received"; cleanup; exit 0' INT TERM

start_service() {
  local label="$1"
  shift
  echo "[local-stack] starting $label: $*"
  "$@" &
  local pid=$!
  PIDS+=("$pid")
  LABELS+=("$label")
}

start_service "cex-notbinance" "$cex_bin" \
  notbinance "$ROOT_DIR/data/notbinance.ndjson" 9001 notbinance-token 150
start_service "cex-notcoinbase" "$cex_bin" \
  notcoinbase "$ROOT_DIR/data/notcoinbase.ndjson" 9002 notcoinbase-token 200
start_service "cex-notkraken" "$cex_bin" \
  notkraken "$ROOT_DIR/data/notkraken.ndjson" 9003 notkraken-token 220

sleep 1
start_service "aggregator" "$agg_bin" "$CONFIG_PATH"

sleep 1
start_service "bbo" "$bbo_bin" "$AGG_ENDPOINT" "$AGG_TOKEN" "$SYMBOL" "$OUTPUT_DIR/bbo/bbo_quotes.csv"
start_service "volume_bands" "$volume_bin" "$AGG_ENDPOINT" "$AGG_TOKEN" "$SYMBOL" "$OUTPUT_DIR/volume_bands/volume_bands.csv"
start_service "price_bands" "$price_bin" "$AGG_ENDPOINT" "$AGG_TOKEN" "$SYMBOL" "$OUTPUT_DIR/price_bands/price_bands.csv"

echo "[local-stack] services running. Press Ctrl-C to stop. Logs stream below."
wait -n || true
