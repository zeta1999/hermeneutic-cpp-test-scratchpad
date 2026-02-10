#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.coverage"}
COMPILER_ID=${COVERAGE_COMPILER:-$(find_compiler_id "$BUILD_DIR")}
case "$COMPILER_ID" in
  *GNU*)
    COVERAGE_FLAGS="--coverage"
    ;;
  *)
    COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
    COMPILER_ID="Clang"
    ;;
 esac
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Debug
  -DHERMENEUTIC_ENABLE_DEBUG_ASSERTS=ON
  "-DCMAKE_C_FLAGS=${COVERAGE_FLAGS}"
  "-DCMAKE_CXX_FLAGS=${COVERAGE_FLAGS}"
  "-DCMAKE_EXE_LINKER_FLAGS=${COVERAGE_FLAGS}"
  "-DCMAKE_SHARED_LINKER_FLAGS=${COVERAGE_FLAGS}"
  "-DCMAKE_MODULE_LINKER_FLAGS=${COVERAGE_FLAGS}"
)
ensure_build_dir "$BUILD_DIR" "${CMAKE_ARGS[@]}"
ensure_directory "$BUILD_DIR/coverage"
if [[ "$COMPILER_ID" == "Clang" ]]; then
  export LLVM_PROFILE_FILE="$BUILD_DIR/coverage/local-stack-%p.profraw"
fi
export BUILD_PARALLEL=${BUILD_PARALLEL:-$(build_jobs)}
exec BUILD_DIR="$BUILD_DIR" "$HERMENEUTIC_ROOT/scripts/run_local_stack_once.sh" "$@"
