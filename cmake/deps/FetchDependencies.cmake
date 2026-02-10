include(FetchContent)

if(FETCHCONTENT_FULLY_DISCONNECTED)
  message(STATUS "Disabling FETCHCONTENT_FULLY_DISCONNECTED for fresh dependency sync")
  set(FETCHCONTENT_FULLY_DISCONNECTED OFF CACHE BOOL "" FORCE)
endif()

function(_hermeneutic_prepare_fetchcontent name)
  string(TOUPPER "${name}" _upper_name)
  set(_source_var "FETCHCONTENT_SOURCE_DIR_${_upper_name}")
  if(DEFINED ${_source_var})
    set(_source_dir "${${_source_var}}")
    if(_source_dir AND NOT EXISTS "${_source_dir}")
      message(STATUS
              "Clearing stale ${_source_var} path '${_source_dir}' (missing)")
      unset(${_source_var} CACHE)
      unset(${_source_var})
    endif()
  endif()

  if(NOT DEFINED ${_source_var} OR "${${_source_var}}" STREQUAL "")
    file(GLOB _hermeneutic_other_builds LIST_DIRECTORIES true
         "${CMAKE_SOURCE_DIR}/build*")
    file(REAL_PATH "${CMAKE_BINARY_DIR}" _resolved_binary)
    foreach(_abs_build_dir IN LISTS _hermeneutic_other_builds)
      if(NOT IS_DIRECTORY "${_abs_build_dir}")
        continue()
      endif()
      file(REAL_PATH "${_abs_build_dir}" _resolved_candidate)
      if(_resolved_candidate STREQUAL "${_resolved_binary}")
        continue()
      endif()
      set(_candidate_dir "${_resolved_candidate}/_deps/${name}-src")
      if(EXISTS "${_candidate_dir}")
        message(STATUS
                "Reusing ${name} sources from '${_candidate_dir}'")
        set(${_source_var} "${_candidate_dir}" CACHE PATH "" FORCE)
        break()
      endif()
    endforeach()
  endif()
endfunction()

set(SPDLOG_VERSION v1.13.0 CACHE STRING "spdlog version")
set(SIMDJSON_VERSION v3.1.8 CACHE STRING "simdjson version")
set(GRPC_VERSION v1.44.0 CACHE STRING "grpc version")
# 1.50 no
# 1.44
# set(GRPC_VERSION v1.62.0 CACHE STRING "grpc version")
set(POCO_VERSION poco-1.13.3-release CACHE STRING "poco version")

_hermeneutic_prepare_fetchcontent(spdlog)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG ${SPDLOG_VERSION}
)

_hermeneutic_prepare_fetchcontent(simdjson)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG ${SIMDJSON_VERSION}
)

set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(gRPC_ZLIB_PROVIDER package CACHE STRING "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_RANDOM_HWAES_X64_FLAGS "" CACHE STRING "" FORCE)
set(ABSL_RANDOM_HWAES_MSVC_X64_FLAGS "" CACHE STRING "" FORCE)
set(ABSL_RANDOM_HWAES_ARM64_FLAGS "" CACHE STRING "" FORCE)
set(ABSL_RANDOM_HWAES_ARM32_FLAGS "" CACHE STRING "" FORCE)
set(ABSL_RANDOM_RANDEN_COPTS "" CACHE STRING "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_TESTING OFF CACHE BOOL "re2 tests" FORCE)
_hermeneutic_prepare_fetchcontent(grpc)
FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG ${GRPC_VERSION}
  GIT_SUBMODULES_RECURSE ON
)

set(POCO_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(POCO_ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
set(POCO_UNBUNDLED ON CACHE BOOL "" FORCE)
_hermeneutic_prepare_fetchcontent(poco)
FetchContent_Declare(
  poco
  GIT_REPOSITORY https://github.com/pocoproject/poco.git
  GIT_TAG ${POCO_VERSION}
)

set(_hermeneutic_saved_build_testing ${BUILD_TESTING})
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog simdjson grpc poco)
set(BUILD_TESTING ${_hermeneutic_saved_build_testing} CACHE BOOL "" FORCE)
unset(_hermeneutic_saved_build_testing)

FetchContent_GetProperties(grpc)
if(grpc_POPULATED)
  set(_hermeneutic_promise_like_header
      "${grpc_SOURCE_DIR}/src/core/lib/promise/detail/promise_like.h")
  if(EXISTS "${_hermeneutic_promise_like_header}")
    file(READ "${_hermeneutic_promise_like_header}" _hermeneutic_promise_like)
    string(REPLACE "typename std::result_of<F()>::type"
                   "absl::result_of_t<F()>"
                   _hermeneutic_promise_like_patched
                   "${_hermeneutic_promise_like}")
    if(NOT _hermeneutic_promise_like_patched STREQUAL
           _hermeneutic_promise_like)
      # gRPC still references std::result_of, which vanished in libc++'s
      # C++20 headers. Abseil provides a portability shim, so rewrite the
      # problematic specialization once after FetchContent populates gRPC.
      file(WRITE "${_hermeneutic_promise_like_header}"
           "${_hermeneutic_promise_like_patched}")
    endif()
  endif()

  set(_hermeneutic_basic_seq_header
      "${grpc_SOURCE_DIR}/src/core/lib/promise/detail/basic_seq.h")
  if(EXISTS "${_hermeneutic_basic_seq_header}")
    file(READ "${_hermeneutic_basic_seq_header}" _hermeneutic_basic_seq)
    string(REPLACE "Traits::template CallSeqFactory(f_, *cur_, std::move(arg))"
                   "Traits::template CallSeqFactory<>(f_, *cur_, std::move(arg))"
                   _hermeneutic_basic_seq_patched
                   "${_hermeneutic_basic_seq}")
    if(NOT _hermeneutic_basic_seq_patched STREQUAL _hermeneutic_basic_seq)
      # Clang warns (and libc++ errors) if template keyword is not followed by
      # an argument list. gRPC upstream fixed this, so keep our vendored copy in
      # sync until we can bump the dependency.
      file(WRITE "${_hermeneutic_basic_seq_header}"
           "${_hermeneutic_basic_seq_patched}")
    endif()
  endif()

  set(_hermeneutic_absl_extension_header
      "${grpc_SOURCE_DIR}/third_party/abseil-cpp/absl/strings/internal/str_format/extension.h")
  if(EXISTS "${_hermeneutic_absl_extension_header}")
    file(READ "${_hermeneutic_absl_extension_header}" _hermeneutic_absl_extension)
    if(_hermeneutic_absl_extension MATCHES "#include <cstdint>")
      # Header already includes <cstdint>; no action required.
    else()
      string(REPLACE "#include <limits.h>"
                     "#include <cstdint>\n#include <limits.h>"
                     _hermeneutic_absl_extension_patched
                     "${_hermeneutic_absl_extension}")
      if(NOT _hermeneutic_absl_extension_patched STREQUAL
             _hermeneutic_absl_extension)
        # GCC 13+ requires <cstdint> for the FormatConversionChar forward
        # declarations. Inject the missing header until we can upgrade gRPC's
        # Abseil snapshot.
        file(WRITE "${_hermeneutic_absl_extension_header}"
             "${_hermeneutic_absl_extension_patched}")
      endif()
    endif()
  endif()
endif()

if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  # Abseil unconditionally injects x86-specific -msse4.1/-maes flags for
  # universal macOS builds. Apple Silicon-only builds choke on those flags, so
  # strip the x86 block from the random/HW AES targets to keep clang happy.
  set(_hermeneutic_absl_randen_targets
      absl_random_internal_randen_hwaes
      absl_random_internal_randen_hwaes_impl)
  foreach(_absl_target IN LISTS _hermeneutic_absl_randen_targets)
    if(TARGET ${_absl_target})
      get_property(_absl_compile_options TARGET ${_absl_target} PROPERTY COMPILE_OPTIONS)
      if(_absl_compile_options)
        set(_absl_filtered_compile_options)
        set(_skip_x86_flags OFF)
        foreach(_absl_flag IN LISTS _absl_compile_options)
          if(_absl_flag MATCHES "^-Xarch_")
            if(_absl_flag STREQUAL "-Xarch_x86_64")
              set(_skip_x86_flags ON)
              continue()
            else()
              set(_skip_x86_flags OFF)
              list(APPEND _absl_filtered_compile_options "${_absl_flag}")
              continue()
            endif()
          endif()
          if(_skip_x86_flags)
            continue()
          endif()
          list(APPEND _absl_filtered_compile_options "${_absl_flag}")
        endforeach()
        if(_absl_filtered_compile_options)
          set_property(TARGET ${_absl_target} PROPERTY COMPILE_OPTIONS ${_absl_filtered_compile_options})
        else()
          set_property(TARGET ${_absl_target} PROPERTY COMPILE_OPTIONS "")
        endif()
      endif()
    endif()
  endforeach()
endif()

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings
  INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

add_library(third_party_doctest INTERFACE)
target_include_directories(third_party_doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)
