#!/usr/bin/env bash
set -euo pipefail

for llvm_dir in /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin; do
  if [ -d "$llvm_dir" ] && [[ ":$PATH:" != *":$llvm_dir:"* ]]; then
    PATH="$llvm_dir:$PATH"
  fi
done

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"

BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.coverage"}
LOCAL_STACK_SCRIPT=${LOCAL_STACK_SCRIPT:-"$HERMENEUTIC_ROOT/scripts/run_local_stack.sh"}
COVERAGE_PREFIX=${COVERAGE_PREFIX:-"local-stack"}
COMPILER_ID=${COVERAGE_COMPILER:-$(find_compiler_id "$BUILD_DIR")}
case "$COMPILER_ID" in
  *GNU*)
    COVERAGE_FLAGS="--coverage"
    TOOLCHAIN="GNU"
    ;;
  *)
    COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
    TOOLCHAIN="Clang"
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

if [[ "$TOOLCHAIN" = "Clang" ]]; then
  rm -f "$BUILD_DIR"/coverage/${COVERAGE_PREFIX}-*.profraw "$BUILD_DIR"/coverage/${COVERAGE_PREFIX}.profdata 2>/dev/null || true
  export LLVM_PROFILE_FILE="$BUILD_DIR/coverage/${COVERAGE_PREFIX}-%p-%m.profraw"
fi

run_stack() {
  export BUILD_PARALLEL=${BUILD_PARALLEL:-$(build_jobs)}
  BUILD_DIR="$BUILD_DIR" "$LOCAL_STACK_SCRIPT" "$@"
}

generate_clang_report() {
  shopt -s nullglob
  local profraws=("$BUILD_DIR"/coverage/${COVERAGE_PREFIX}-*.profraw)
  shopt -u nullglob
  if [ ${#profraws[@]} -eq 0 ]; then
    printf '%s\n' "[coverage] No ${COVERAGE_PREFIX} .profraw files were generated" >&2
    return 0
  fi
  local llvm_profdata=$(command -v llvm-profdata || true)
  local llvm_cov=$(command -v llvm-cov || true)
  if [ -z "$llvm_profdata" ] || [ -z "$llvm_cov" ]; then
    printf '%s\n' "[coverage] llvm-profdata/llvm-cov not available; raw profiles left under $BUILD_DIR/coverage" >&2
    return 0
  fi
  local profdata="$BUILD_DIR/coverage/${COVERAGE_PREFIX}.profdata"
  "$llvm_profdata" merge -sparse "${profraws[@]}" -o "$profdata"
  local binaries=()
  while IFS= read -r -d '' bin; do
    binaries+=("$bin")
  done < <(find "$BUILD_DIR" -type f -perm -111 \( -path "$BUILD_DIR/tests/*" -o -path "$BUILD_DIR/services/*" \) -print0)
  if [ ${#binaries[@]} -eq 0 ]; then
    printf '%s\n' "[coverage] No binaries discovered for llvm-cov report" >&2
    return 0
  fi
  local cov_objects=()
  for bin in "${binaries[@]}"; do
    cov_objects+=("-object" "$bin")
  done
  "$llvm_cov" report -ignore-filename-regex "/_deps/" "${cov_objects[@]}" \
    -instr-profile "$profdata" \
    > "$BUILD_DIR/coverage/llvm-cov-report.txt"
  printf '%s\n' "[coverage] LLVM coverage report written to $BUILD_DIR/coverage/llvm-cov-report.txt"
  printf '%s\n' "[coverage] Summary (first 20 lines):"
  head -n 20 "$BUILD_DIR/coverage/llvm-cov-report.txt"
  cat "$BUILD_DIR/coverage/llvm-cov-report.txt"
}

generate_gnu_report() {
  if ! command -v gcovr >/dev/null 2>&1; then
    printf '%s\n' "[coverage] gcovr not found; .gcda files available under $BUILD_DIR" >&2
    return 0
  fi
  gcovr \
    -r "$HERMENEUTIC_ROOT" \
    --exclude '.*_deps/.*' \
    --exclude '.*/third_party/.*' \
    --exclude '.*/SQLParser/.*' \
    --exclude '.*/proto/grpc/channelz/.*' \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-ignore-parse-errors=all \
    "$BUILD_DIR" > "$BUILD_DIR/coverage/gcovr-report.txt"
  printf '%s\n' "[coverage] gcovr report written to $BUILD_DIR/coverage/gcovr-report.txt"
  cat "$BUILD_DIR/coverage/gcovr-report.txt"
}

STACK_STATUS=0
if ! run_stack "$@"; then
  STACK_STATUS=$?
fi

COVERAGE_STATUS=0
if [[ "$TOOLCHAIN" = "Clang" ]]; then
  if ! generate_clang_report; then
    COVERAGE_STATUS=$?
  fi
else
  if ! generate_gnu_report; then
    COVERAGE_STATUS=$?
  fi
fi

if [ "$STACK_STATUS" -ne 0 ]; then
  exit "$STACK_STATUS"
fi
exit "$COVERAGE_STATUS"
