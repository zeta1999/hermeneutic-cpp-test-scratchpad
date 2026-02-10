#!/bin/sh
# Run the compose demo after ensuring CSV output directories exist.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

OUTPUT_DIR="$ROOT_DIR/output"
mkdir -p "$OUTPUT_DIR/bbo" "$OUTPUT_DIR/volume_bands" "$OUTPUT_DIR/price_bands"
: > "$OUTPUT_DIR/bbo/bbo_quotes.csv"
: > "$OUTPUT_DIR/volume_bands/volume_bands.csv"
: > "$OUTPUT_DIR/price_bands/price_bands.csv"

echo "[compose] writing CSV outputs under $OUTPUT_DIR"
if [ "$#" -gt 0 ] && [ "$1" = "--build" ]; then
  shift
  echo "[compose] rebuilding images via docker compose"
  docker compose -f docker/compose.yml build
  exec docker compose -f docker/compose.yml up "$@"
else
  exec docker compose -f docker/compose.yml "$@"
fi
