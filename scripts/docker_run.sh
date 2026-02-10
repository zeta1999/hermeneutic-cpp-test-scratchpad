#!/bin/sh
# Run the compose demo after ensuring CSV output directories exist.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

OUTPUT_DIR="$ROOT_DIR/output"
mkdir -p "$OUTPUT_DIR/bbo" "$OUTPUT_DIR/volume_bands" "$OUTPUT_DIR/price_bands"

echo "[compose] writing CSV outputs under $OUTPUT_DIR"
exec docker compose -f docker/compose.yml "$@"
