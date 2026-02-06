add_library(lob STATIC)
target_sources(lob
  PUBLIC
    include/hermeneutic/lob/order_book.hpp
  PRIVATE
    order_book.cpp
)
target_include_directories(lob PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lob PUBLIC common)
