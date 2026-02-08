add_executable(volume_bands_service main.cpp)
target_link_libraries(volume_bands_service PRIVATE volume_bands aggregator_proto grpc++)
target_include_directories(volume_bands_service PRIVATE ${CMAKE_SOURCE_DIR}/services)
