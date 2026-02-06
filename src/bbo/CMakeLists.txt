add_library(bbo STATIC)
target_sources(bbo
  PUBLIC
    include/hermeneutic/bbo/bbo_publisher.hpp
  PRIVATE
    bbo_publisher.cpp
)
target_include_directories(bbo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bbo PUBLIC common spdlog::spdlog)
