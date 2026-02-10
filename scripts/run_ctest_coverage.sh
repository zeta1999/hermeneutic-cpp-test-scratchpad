#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.coverage"}
COMPILER_ID=${COVERAGE_COMPILER:-$(find_compiler_id "$BUILD_DIR")}
case "$COMPILER_ID" in
  *GNU*)
    COVERAGE_FLAGS="--coverage"
    TOOLCHAIN="GNU"
    ;;
  *)
    COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
    TOOLCHAIN="Clang"
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
cmake --build "$BUILD_DIR" --parallel "${BUILD_PARALLEL:-$(build_jobs)}"
ensure_directory "$BUILD_DIR/coverage"
if [ "$TOOLCHAIN" = "Clang" ]; then
  export LLVM_PROFILE_FILE="$BUILD_DIR/coverage/ctest-%p.profraw"
fi
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$(ctest_jobs)"
if [ "$TOOLCHAIN" = "Clang" ]; then
  shopt -s nullglob
  profraws=($BUILD_DIR/coverage/*.profraw)
  shopt -u nullglob
  if [ ${#profraws[@]} -eq 0 ]; then
    echo "[coverage] No .profraw files were generated" >&2
    exit 0
  fi
  llvm_profdata=$(command -v llvm-profdata || true)
  llvm_cov=$(command -v llvm-cov || true)
  if [ -z "$llvm_profdata" ] || [ -z "$llvm_cov" ]; then
    echo "[coverage] llvm-profdata/llvm-cov not available; raw profiles left under $BUILD_DIR/coverage" >&2
    exit 0
  fi
  "$llvm_profdata" merge -sparse "${profraws[@]}" -o "$BUILD_DIR/coverage/coverage.profdata"
  mapfile -t binaries < <(find "$BUILD_DIR" -type f -perm -111 \( -path "$BUILD_DIR/tests/*" -o -path "$BUILD_DIR/services/*" \))
  if [ ${#binaries[@]} -eq 0 ]; then
    echo "[coverage] No binaries discovered for llvm-cov report" >&2
    exit 0
  fi
  "$llvm_cov" report "${binaries[@]}" -instr-profile "$BUILD_DIR/coverage/coverage.profdata" \
    > "$BUILD_DIR/coverage/llvm-cov-report.txt"
  echo "[coverage] LLVM coverage report written to $BUILD_DIR/coverage/llvm-cov-report.txt"
else
  if command -v gcovr >/dev/null 2>&1; then
    gcovr -r "$HERMENEUTIC_ROOT" "$BUILD_DIR" > "$BUILD_DIR/coverage/gcovr-report.txt"
    echo "[coverage] gcovr report written to $BUILD_DIR/coverage/gcovr-report.txt"
  else
    echo "[coverage] gcovr not found; .gcda files available under $BUILD_DIR" >&2
  fi
fi
