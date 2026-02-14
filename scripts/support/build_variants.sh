#!/usr/bin/env bash
set -euo pipefail

if [ -z "${HERMENEUTIC_ROOT:-}" ]; then
  HERMENEUTIC_ROOT=$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
fi

detect_core_count() {
  local count
  if command -v sysctl >/dev/null 2>&1; then
    count=$(sysctl -n hw.ncpu 2>/dev/null || true)
  fi
  if [ -z "$count" ] && command -v nproc >/dev/null 2>&1; then
    count=$(nproc 2>/dev/null || true)
  fi
  if [[ ! "$count" =~ ^[0-9]+$ ]] || [ "$count" -le 0 ]; then
    count=8
  fi
  if [ "$count" -gt 8 ]; then
    count=8
  fi
  printf '%s' "$count"
}

DEFAULT_BUILD_CORES=$(detect_core_count)

ensure_build_dir() {
  local build_dir=$1
  shift || true
  if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    cmake -S "$HERMENEUTIC_ROOT" -B "$build_dir" "$@"
  fi
}

build_jobs() {
  echo "${BUILD_PARALLEL:-$DEFAULT_BUILD_CORES}"
}

ctest_jobs() {
  echo "${CTEST_JOBS:-$DEFAULT_BUILD_CORES}"
}

sanitize_env_default() {
  local kind=$1
  case "$kind" in
    asan)
      local leak_opt="detect_leaks=1"
      if [[ "${OSTYPE:-}" == darwin* ]]; then
        leak_opt="detect_leaks=0"
      fi
      export ASAN_OPTIONS=${ASAN_OPTIONS:-"$leak_opt"}
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
