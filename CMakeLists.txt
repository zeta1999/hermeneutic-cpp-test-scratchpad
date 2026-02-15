cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_POLICY_VERSION)
  set(CMAKE_POLICY_VERSION 3.24)
endif()
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()
project(hermeneutic LANGUAGES C CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "ccache found: ${CCACHE_PROGRAM}")
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
  message(STATUS "ccache not found; compiling without compiler launcher")
endif()

if(NOT DEFINED FETCHCONTENT_BASE_DIR OR FETCHCONTENT_BASE_DIR STREQUAL "")
  if(DEFINED ENV{HERMENEUTIC_DEPS_DIR} AND NOT "$ENV{HERMENEUTIC_DEPS_DIR}" STREQUAL "")
    set(FETCHCONTENT_BASE_DIR "$ENV{HERMENEUTIC_DEPS_DIR}")
  else()
    if(NOT CMAKE_HOST_SYSTEM_NAME)
      set(CMAKE_HOST_SYSTEM_NAME "unknown")
    endif()
    string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" _hermeneutic_host_os)
    set(FETCHCONTENT_BASE_DIR
        "${CMAKE_SOURCE_DIR}/.deps-cache/${_hermeneutic_host_os}")
  endif()
endif()
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
message(STATUS "FetchContent base dir: ${FETCHCONTENT_BASE_DIR}")

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTING "Build the unit test suite" ON)
option(HERMENEUTIC_ALLOW_TESTLESS_BUILDS
       "Permit configuring without the project's tests"
       OFF)
option(HERMENEUTIC_FETCH_DEPS_ONLY
       "If ON, stop configuration after FetchContent populates dependencies"
       OFF)

set(HERMENEUTIC_DOCKER_SUFFIX "$ENV{HERMENEUTIC_DOCKER_SUFFIX}")

set(_hermeneutic_assert_default OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_hermeneutic_assert_default ON)
endif()
option(HERMENEUTIC_ENABLE_DEBUG_ASSERTS
       "Enable hermeneutic's debug assertions outside of Debug builds"
       ${_hermeneutic_assert_default})

set(HERMENEUTIC_DECIMAL_BACKEND "int128"
    CACHE STRING "Decimal backend (int128, double, wide)")
set_property(CACHE HERMENEUTIC_DECIMAL_BACKEND PROPERTY STRINGS
             int128 double wide)

if(NOT BUILD_TESTING AND NOT HERMENEUTIC_ALLOW_TESTLESS_BUILDS)
  message(STATUS
          "BUILD_TESTING was OFF; forcing it ON so ctest metadata is generated."
          " Set HERMENEUTIC_ALLOW_TESTLESS_BUILDS=ON to bypass this guard.")
  set(BUILD_TESTING ON CACHE BOOL "" FORCE)
endif()

include(cmake/deps/FetchDependencies.cmake)

if(HERMENEUTIC_FETCH_DEPS_ONLY)
  message(STATUS "Fetch-only configuration requested; skipping target registration")
  return()
endif()

set(_hermeneutic_decimal_define HERMENEUTIC_DECIMAL_BACKEND_INT128)
if(HERMENEUTIC_DECIMAL_BACKEND STREQUAL "double")
  set(_hermeneutic_decimal_define HERMENEUTIC_DECIMAL_BACKEND_DOUBLE)
elseif(HERMENEUTIC_DECIMAL_BACKEND STREQUAL "wide")
  set(_hermeneutic_decimal_define HERMENEUTIC_DECIMAL_BACKEND_WIDE)
elseif(NOT HERMENEUTIC_DECIMAL_BACKEND STREQUAL "int128")
  message(FATAL_ERROR
          "Unknown HERMENEUTIC_DECIMAL_BACKEND='${HERMENEUTIC_DECIMAL_BACKEND}'. "
          "Use int128, double, or wide.")
endif()
target_compile_definitions(project_options
  INTERFACE
    ${_hermeneutic_decimal_define}=1
    HERMENEUTIC_ENABLE_DEBUG_ASSERTS=$<BOOL:${HERMENEUTIC_ENABLE_DEBUG_ASSERTS}>
)

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(services)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

add_custom_target(docker-images
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.aggregator${HERMENEUTIC_DOCKER_SUFFIX} -t hermeneutic/aggregator .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.bbo${HERMENEUTIC_DOCKER_SUFFIX} -t hermeneutic/bbo .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.volume${HERMENEUTIC_DOCKER_SUFFIX} -t hermeneutic/volume .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.price${HERMENEUTIC_DOCKER_SUFFIX} -t hermeneutic/price .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.cex${HERMENEUTIC_DOCKER_SUFFIX} -t hermeneutic/cex .
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Build all service docker images"
)
