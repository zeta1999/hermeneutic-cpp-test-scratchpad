add_library(price_bands STATIC)
target_sources(price_bands
  PUBLIC
    include/hermeneutic/price_bands/price_bands_publisher.hpp
  PRIVATE
    price_bands_publisher.cpp
)
target_include_directories(price_bands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(price_bands PUBLIC common)
