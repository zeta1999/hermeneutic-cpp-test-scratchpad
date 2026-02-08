add_executable(bbo_service main.cpp)
target_link_libraries(bbo_service PRIVATE bbo aggregator_proto grpc++)
target_include_directories(bbo_service PRIVATE ${CMAKE_SOURCE_DIR}/services)
