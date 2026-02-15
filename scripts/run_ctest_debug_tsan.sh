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

find_protoc_in_build() {
  local search_dir=$1
  if [ -d "$search_dir" ]; then
    find "$search_dir" -path '*third_party/protobuf/protoc-*' -type f -perm -111 2>/dev/null | head -n1 || true
  fi
}

ensure_non_sanitized_protoc() {
  local existing=${PROTOC_BIN:-${PROTOC:-}}
  if [ -n "$existing" ]; then
    printf '%s' "$existing"
    return 0
  fi

  local base_dir=${PROTOC_BUILD_DIR:-"$HERMENEUTIC_ROOT/build"}
  local candidate=$(find_protoc_in_build "$base_dir")
  if [ -z "$candidate" ]; then
    echo "[tsan] building host protoc in $base_dir" >&2
    if [ ! -f "$base_dir/CMakeCache.txt" ]; then
      cmake -S "$HERMENEUTIC_ROOT" -B "$base_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    fi
    cmake --build "$base_dir" --target protoc --parallel "$(build_jobs)"
    candidate=$(find_protoc_in_build "$base_dir")
  fi
  if [ -z "$candidate" ]; then
    candidate=$(command -v protoc || true)
  fi
  if [ -z "$candidate" ]; then
    echo "[tsan] error: unable to locate a non-sanitized protoc binary." >&2
    echo "       Run a normal build (e.g. cmake --preset relwithdebinfo && cmake --build build) before running this script." >&2
    exit 1
  fi
  printf '%s' "$candidate"
}

PROTOC_BIN=$(ensure_non_sanitized_protoc)
CMAKE_ARGS+=("-DProtobuf_PROTOC_EXECUTABLE=$PROTOC_BIN")
ensure_build_dir "$BUILD_DIR" "${CMAKE_ARGS[@]}"
sanitize_env_default tsan
echo "$BUILD_DIR" "${CMAKE_ARGS[@]}"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DHERMENEUTIC_ENABLE_DEBUG_ASSERTS=ON \
    "-DPROTOBUF_PROTOC_EXECUTABLE=$PROTOC_BIN"

cmake --build "$BUILD_DIR" --parallel "${BUILD_PARALLEL:-$(build_jobs)}" 
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "$(ctest_jobs)" 
