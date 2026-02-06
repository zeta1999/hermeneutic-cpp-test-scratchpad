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
