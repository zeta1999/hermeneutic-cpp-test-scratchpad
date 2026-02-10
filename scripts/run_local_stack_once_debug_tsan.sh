#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.debug-tsan"}
SAN_FLAGS="-fsanitize=thread"
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
sanitize_env_default tsan
export BUILD_PARALLEL=${BUILD_PARALLEL:-$(build_jobs)}
exec BUILD_DIR="$BUILD_DIR" "$HERMENEUTIC_ROOT/scripts/run_local_stack_once.sh" "$@"
