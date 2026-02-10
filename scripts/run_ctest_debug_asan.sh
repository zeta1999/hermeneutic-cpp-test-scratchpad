#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.debug-asan"}
SAN_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Debug
  -DHERMENEUTIC_ENABLE_DEBUG_ASSERTS=ON
  "-DCMAKE_C_FLAGS=${SAN_FLAGS}"
  "-DCMAKE_CXX_FLAGS=${SAN_FLAGS}"
  "-DCMAKE_EXE_LINKER_FLAGS=${SAN_FLAGS}"
  "-DCMAKE_SHARED_LINKER_FLAGS=${SAN_FLAGS}"
  "-DCMAKE_MODULE_LINKER_FLAGS=${SAN_FLAGS}"
)
ensure_build_dir "$BUILD_DIR" "${CMAKE_ARGS[@]}"
sanitize_env_default asan
cmake --build "$BUILD_DIR" --parallel "${BUILD_PARALLEL:-$(build_jobs)}"
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$(ctest_jobs)"
