#!/bin/sh
# Run the compose demo after ensuring CSV output directories exist.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

DOCKER_SUFFIX=${HERMENEUTIC_DOCKER_SUFFIX:-}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --alpine)
      DOCKER_SUFFIX=".alpine"
      shift
      ;;
    --docker-suffix)
      shift
      if [ "$#" -eq 0 ]; then
        echo "[compose] --docker-suffix expects a value" >&2
        exit 1
      fi
      DOCKER_SUFFIX="$1"
      if [ -n "$DOCKER_SUFFIX" ] && [ "${DOCKER_SUFFIX#*.}" = "$DOCKER_SUFFIX" ]; then
        DOCKER_SUFFIX=".$DOCKER_SUFFIX"
      fi
      shift
      ;;
    --)
      shift
      break
      ;;
    --build)
      break
      ;;
    *)
      break
      ;;
  esac
done

export HERMENEUTIC_DOCKER_SUFFIX="$DOCKER_SUFFIX"

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
