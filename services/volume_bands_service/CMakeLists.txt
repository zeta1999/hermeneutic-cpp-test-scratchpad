add_executable(volume_bands_service main.cpp)
target_link_libraries(volume_bands_service PRIVATE volume_bands common aggregator_proto grpc++ spdlog::spdlog)
target_include_directories(volume_bands_service PRIVATE ${CMAKE_SOURCE_DIR}/services)
