add_executable(aggregator_service main.cpp)
target_link_libraries(aggregator_service PRIVATE aggregator cex_type1 aggregator_proto grpc++)
target_include_directories(aggregator_service PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/services)
