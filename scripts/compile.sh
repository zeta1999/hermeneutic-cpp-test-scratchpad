#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"

BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build"}
BUILD_TYPE=${BUILD_TYPE:-"RelWithDebInfo"}
PARALLEL=${BUILD_PARALLEL:-$(build_jobs)}

CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

echo "[build-local-stack] configuring CMake build directory at $BUILD_DIR"
ensure_build_dir "$BUILD_DIR" "${CMAKE_ARGS[@]}"

TARGETS=(
  cex_type1_service
  aggregator_service
  bbo_service
  volume_bands_service
  price_bands_service
)

build_cmd=(cmake --build "$BUILD_DIR")
for target in "${TARGETS[@]}"; do
  build_cmd+=(--target "$target")
done
if [ -n "$PARALLEL" ]; then
  build_cmd+=(--parallel "$PARALLEL")
fi

echo "[build-local-stack] building service targets (${TARGETS[*]})"
"${build_cmd[@]}"
