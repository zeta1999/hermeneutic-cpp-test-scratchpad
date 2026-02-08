function(add_project_test target)
  cmake_parse_arguments(TGT "" "" "SOURCES;LIBS" ${ARGN})
  add_executable(${target} ${TGT_SOURCES})
  target_link_libraries(${target} PRIVATE third_party_doctest ${TGT_LIBS})
  target_compile_definitions(${target}
    PRIVATE
      PROJECT_BINARY_DIR="${CMAKE_BINARY_DIR}"
      PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_project_test(test_decimal SOURCES common/test_decimal.cpp LIBS common)
add_project_test(test_enum SOURCES common/test_enum.cpp LIBS common)
add_project_test(test_order_book SOURCES lob/test_order_book.cpp LIBS lob)
add_project_test(test_aggregator SOURCES aggregator/test_aggregator.cpp LIBS aggregator)
add_project_test(test_cex_loopback SOURCES cex_type1/test_loopback.cpp LIBS cex_type1)
add_project_test(test_cex_type1_service SOURCES cex_type1/test_cex_type1_service.cpp LIBS cex_type1)
add_project_test(test_cex_reconnect SOURCES cex_type1/test_reconnect.cpp LIBS cex_type1)
