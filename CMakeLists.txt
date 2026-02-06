cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_POLICY_VERSION)
  set(CMAKE_POLICY_VERSION 3.24)
endif()
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()
project(hermeneutic LANGUAGES C CXX)

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/deps/FetchDependencies.cmake)

option(BUILD_TESTING "Build the unit test suite" ON)

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(services)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

add_custom_target(docker-images
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.aggregator -t hermeneutic/aggregator .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.bbo -t hermeneutic/bbo .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.volume -t hermeneutic/volume .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.price -t hermeneutic/price .
  COMMAND ${CMAKE_COMMAND} -E env DOCKER_BUILDKIT=1 docker build -f docker/Dockerfile.cex -t hermeneutic/cex .
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Build all service docker images"
)
