add_library(cex_type1 STATIC)
target_sources(cex_type1
  PUBLIC
    include/hermeneutic/cex_type1/feed.hpp
  PRIVATE
    feed.cpp
)
target_include_directories(cex_type1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cex_type1 PUBLIC common spdlog::spdlog simdjson::simdjson Poco::Net Poco::Util Poco::Foundation)
