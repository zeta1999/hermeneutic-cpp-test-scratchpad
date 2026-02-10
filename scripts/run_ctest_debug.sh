#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.debug"}
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Debug
  -DHERMENEUTIC_ENABLE_DEBUG_ASSERTS=ON
)
ensure_build_dir "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel "${BUILD_PARALLEL:-$(build_jobs)}"
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$(ctest_jobs)"
