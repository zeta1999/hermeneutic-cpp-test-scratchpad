add_executable(price_bands_service main.cpp)
target_link_libraries(price_bands_service PRIVATE price_bands common aggregator_proto grpc++ spdlog::spdlog)
target_include_directories(price_bands_service PRIVATE ${CMAKE_SOURCE_DIR}/services)
