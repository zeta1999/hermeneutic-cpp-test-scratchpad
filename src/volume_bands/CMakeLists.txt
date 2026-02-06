add_library(volume_bands STATIC)
target_sources(volume_bands
  PUBLIC
    include/hermeneutic/volume_bands/volume_bands_publisher.hpp
  PRIVATE
    volume_bands_publisher.cpp
)
target_include_directories(volume_bands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(volume_bands PUBLIC common)
