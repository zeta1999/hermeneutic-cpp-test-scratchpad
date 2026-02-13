add_library(aggregator_service_support STATIC feed_wait.cpp)
target_link_libraries(aggregator_service_support
  PUBLIC
    spdlog::spdlog
    Poco::Foundation
    aggregator
)
target_include_directories(aggregator_service_support
  PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/services
)

add_executable(aggregator_service main.cpp)
target_link_libraries(aggregator_service
  PRIVATE
    aggregator_grpc
    cex_type1
    aggregator
    grpc++
    aggregator_service_support
    Poco::Net
)
target_include_directories(aggregator_service PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/services)
