add_library(common STATIC)
target_sources(common
  PUBLIC
    common/include/hermeneutic/common/decimal.hpp
    common/include/hermeneutic/common/events.hpp
    common/include/hermeneutic/common/concurrent_queue.hpp
  PRIVATE
    common/decimal.cpp
)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(common PUBLIC project_options project_warnings spdlog::spdlog)

add_subdirectory(lob)
add_subdirectory(aggregator)
add_subdirectory(cex_type1)
add_subdirectory(bbo)
add_subdirectory(volume_bands)
add_subdirectory(price_bands)
