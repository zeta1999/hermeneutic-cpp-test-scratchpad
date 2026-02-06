add_executable(aggregator_service main.cpp)
target_link_libraries(aggregator_service PRIVATE aggregator cex_type1 aggregator_proto grpc++ spdlog::spdlog)
target_include_directories(aggregator_service PRIVATE ${CMAKE_SOURCE_DIR}/services)
