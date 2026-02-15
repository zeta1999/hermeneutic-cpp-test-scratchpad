#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/support/build_variants.sh"
BUILD_DIR=${BUILD_DIR:-"$HERMENEUTIC_ROOT/build.debug-tsan"}
SAN_FLAGS="-fsanitize=thread"
PROTOC_CCACHE_ARGS=()
if command -v ccache >/dev/null 2>&1; then
  PROTOC_CCACHE_ARGS+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache")
  PROTOC_CCACHE_ARGS+=("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
fi
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

default_cache_root() {
  if [ -n "${HERMENEUTIC_DEPS_DIR:-}" ]; then
    printf '%s' "$HERMENEUTIC_DEPS_DIR"
    return
  fi
  local host_os="$(uname -s 2>/dev/null || echo unknown)"
  host_os=$(printf '%s' "$host_os" | tr '[:upper:]' '[:lower:]')
  printf '%s' "$HERMENEUTIC_ROOT/.deps-cache/$host_os"
}

default_protoc_build_dir() {
  if [ -n "${PROTOC_BUILD_DIR:-}" ]; then
    printf '%s' "$PROTOC_BUILD_DIR"
  else
    printf '%s' "$(default_cache_root)/host-protoc"
  fi
}

ensure_non_sanitized_protoc() {
  local existing=${PROTOC_BIN:-${PROTOC:-}}
  if [ -n "$existing" ]; then
    printf '%s' "$existing"
    return 0
  fi

  local base_dir
  base_dir=$(default_protoc_build_dir)
  ensure_directory "$base_dir"
  local candidate=$(find_protoc_in_build "$base_dir")
  if [ -z "$candidate" ]; then
    echo "[tsan] building host protoc in $base_dir" >&2
    cmake -S "$HERMENEUTIC_ROOT" -B "$base_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DFETCHCONTENT_BASE_DIR="$(default_cache_root)" \
      "${PROTOC_CCACHE_ARGS[@]}"
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
