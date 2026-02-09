add_library(services_common STATIC)
target_sources(services_common
  PUBLIC
    book_stream_client.hpp
    grpc_helpers.hpp
  PRIVATE
    book_stream_client.cpp
)
target_include_directories(services_common PUBLIC
  ${CMAKE_SOURCE_DIR}/services
  ${CMAKE_SOURCE_DIR}/src/common/include
)
target_link_libraries(services_common PUBLIC aggregator_proto grpc++ common)
