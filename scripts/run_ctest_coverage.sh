#!/usr/bin/env bash
set -euo pipefail

# Make Homebrew-installed LLVM tools discoverable (common on macOS).
for llvm_dir in /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin; do
  if [ -d "$llvm_dir" ] && [[ ":$PATH:" != *":$llvm_dir:"* ]]; then
    PATH="$llvm_dir:$PATH"
  fi
done
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
  rm -f "$BUILD_DIR"/coverage/*.profraw "$BUILD_DIR"/coverage/*.profdata 2>/dev/null || true
  export LLVM_PROFILE_FILE="$BUILD_DIR/coverage/ctest-%p-%m.profraw"
fi
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$(ctest_jobs)"
if [ "$TOOLCHAIN" = "Clang" ]; then
  shopt -s nullglob
  profraws=($BUILD_DIR/coverage/*.profraw)
  shopt -u nullglob
  if [ ${#profraws[@]} -eq 0 ]; then
    printf '%s\n' "[coverage] No .profraw files were generated" >&2
    exit 0
  fi
  llvm_profdata=$(command -v llvm-profdata || true)
  llvm_cov=$(command -v llvm-cov || true)
  if [ -z "$llvm_profdata" ] || [ -z "$llvm_cov" ]; then
    printf '%s\n' "[coverage] llvm-profdata/llvm-cov not available; raw profiles left under $BUILD_DIR/coverage" >&2
    exit 0
  fi
  "$llvm_profdata" merge -sparse "${profraws[@]}" -o "$BUILD_DIR/coverage/coverage.profdata"
  binaries=()
  while IFS= read -r -d '' bin; do
    binaries+=("$bin")
  done < <(find "$BUILD_DIR" -type f -perm -111 \( -path "$BUILD_DIR/tests/*" -o -path "$BUILD_DIR/services/*" \) -print0)
  if [ ${#binaries[@]} -eq 0 ]; then
    printf '%s\n' "[coverage] No binaries discovered for llvm-cov report" >&2
    exit 0
  fi
  cov_objects=()
  for bin in "${binaries[@]}"; do
    cov_objects+=("-object" "$bin")
  done
  # Ignore vendored targets (build/_deps) and cached dependency trees (.deps-*) so
  # coverage only reflects our sources.
  # gcovr on Linux complains if filters use backslashes (interpreted as Windows
  # path separators), so prefer character classes like "[.]" instead of "\\.".
  IGNORE_PATTERNS=(
    "/_deps/"
    "/[.]deps-cache/"
    "/[.]deps-[^/]+/"
    "/deps-cache/"
    "/deps-[^/]+/"
  )
  llvm_cov_cmd=("$llvm_cov" report)
  for pat in "${IGNORE_PATTERNS[@]}"; do
    llvm_cov_cmd+=(-ignore-filename-regex "$pat")
  done
  llvm_cov_cmd+=("${cov_objects[@]}" -instr-profile "$BUILD_DIR/coverage/coverage.profdata")
  "${llvm_cov_cmd[@]}" > "$BUILD_DIR/coverage/llvm-cov-report.txt"
  printf '%s\n' "[coverage] LLVM coverage report written to $BUILD_DIR/coverage/llvm-cov-report.txt"
  printf '%s\n' "[coverage] Summary (first 20 lines):"
  head -n 20 "$BUILD_DIR/coverage/llvm-cov-report.txt"
  cat "$BUILD_DIR/coverage/llvm-cov-report.txt"
else
  if command -v gcovr >/dev/null 2>&1; then
    # gcovr takes explicit exclude globs; keep the vendored tree and .deps-* cache out.
    gcovr \
      -r "$HERMENEUTIC_ROOT" \
      --exclude '.*_deps/.*' \
      --exclude 'deps-cache/.*' \
      --exclude '.*/[.]deps-.*' \
      --exclude '.*/third_party/.*' \
      --exclude '.*/SQLParser/.*' \
      --exclude '.*/proto/grpc/channelz/.*' \
      --gcov-ignore-errors=no_working_dir_found \
      --gcov-ignore-parse-errors=all \
      "$BUILD_DIR" > "$BUILD_DIR/coverage/gcovr-report.txt"
    printf '%s\n' "[coverage] gcovr report written to $BUILD_DIR/coverage/gcovr-report.txt"
    cat "$BUILD_DIR/coverage/gcovr-report.txt"
  else
    printf '%s\n' "[coverage] gcovr not found; .gcda files available under $BUILD_DIR" >&2
  fi
fi
