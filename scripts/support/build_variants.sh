#!/usr/bin/env bash
set -euo pipefail

if [ -z "${HERMENEUTIC_ROOT:-}" ]; then
  HERMENEUTIC_ROOT=$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
fi

ensure_build_dir() {
  local build_dir=$1
  shift || true
  if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    cmake -S "$HERMENEUTIC_ROOT" -B "$build_dir" "$@"
  fi
}

build_jobs() {
  echo "${BUILD_PARALLEL:-8}"
}

ctest_jobs() {
  echo "${CTEST_JOBS:-8}"
}

sanitize_env_default() {
  local kind=$1
  case "$kind" in
    asan)
      export ASAN_OPTIONS=${ASAN_OPTIONS:-"detect_leaks=1"}
      ;;
    tsan)
      export TSAN_OPTIONS=${TSAN_OPTIONS:-"halt_on_error=1"}
      ;;
  esac
}

find_compiler_id() {
  local build_dir=$1
  if [ -f "$build_dir/CMakeCache.txt" ]; then
    local id
    id=$(grep -E "^CMAKE_CXX_COMPILER_ID:STRING=" "$build_dir/CMakeCache.txt" | head -n1 | cut -d'=' -f2)
    if [ -n "$id" ]; then
      printf '%s' "$id"
      return 0
    fi
  fi
  local compiler=${CXX:-$(command -v c++ || command -v g++ || true)}
  if [ -z "$compiler" ]; then
    printf '%s' "Clang"
    return 0
  fi
  local version="$($compiler --version 2>/dev/null || true)"
  if echo "$version" | grep -qi clang; then
    printf '%s' "Clang"
  else
    printf '%s' "GNU"
  fi
}

ensure_directory() {
  local dir=$1
  if [ ! -d "$dir" ]; then
    mkdir -p "$dir"
  fi
}
