add_library(aggregator STATIC)
target_sources(aggregator
  PUBLIC
    include/hermeneutic/aggregator/aggregator.hpp
    include/hermeneutic/aggregator/config.hpp
  PRIVATE
    aggregator.cpp
)
target_include_directories(aggregator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aggregator PUBLIC common lob spdlog::spdlog simdjson::simdjson)

add_library(aggregator_grpc STATIC)
target_sources(aggregator_grpc
  PUBLIC
    include/hermeneutic/aggregator/grpc_service.hpp
  PRIVATE
    grpc_service.cpp
)
target_include_directories(aggregator_grpc PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}
)
target_link_libraries(aggregator_grpc PUBLIC aggregator aggregator_proto grpc++)
