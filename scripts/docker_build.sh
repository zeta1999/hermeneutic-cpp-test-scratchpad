#!/bin/sh
# Build all service images with buildx so we can target multiple architectures.
# Usage:
#   PLATFORMS=linux/amd64 scripts/docker_build.sh
#   PLATFORMS=linux/amd64,linux/arm64 OUTPUT=push scripts/docker_build.sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

PLATFORMS=${PLATFORMS:-"linux/amd64"}
BUILDER_NAME=${BUILDER_NAME:-"hermeneutic-builder"}
OUTPUT=${OUTPUT:-load}

case "$OUTPUT" in
  load) OUTPUT_FLAG="--load" ;;
  push) OUTPUT_FLAG="--push" ;;
  *) OUTPUT_FLAG="--output=$OUTPUT" ;;
esac

case "$OUTPUT" in
  load)
    if printf %s "$PLATFORMS" | grep -q ','; then
      echo "[error] OUTPUT=load only works with a single platform."
      echo "        Set OUTPUT=push or OUTPUT=type=oci,dest=... for multi-arch builds." >&2
      exit 1
    fi
    ;;
esac

if ! docker buildx inspect "$BUILDER_NAME" >/dev/null 2>&1; then
  docker buildx create --name "$BUILDER_NAME" --driver docker-container >/dev/null
fi
docker buildx use "$BUILDER_NAME" >/dev/null

build_image() {
  image_tag=$1
  dockerfile=$2
  echo "[buildx] building $image_tag from $dockerfile for $PLATFORMS"
  docker buildx build \
    --platform "$PLATFORMS" \
    --progress=plain \
    -t "$image_tag" \
    -f "$dockerfile" \
    $OUTPUT_FLAG \
    .
}

build_image hermeneutic/cex docker/Dockerfile.cex
build_image hermeneutic/aggregator docker/Dockerfile.aggregator
build_image hermeneutic/bbo docker/Dockerfile.bbo
build_image hermeneutic/volume docker/Dockerfile.volume
build_image hermeneutic/price docker/Dockerfile.price

echo "[buildx] done"
