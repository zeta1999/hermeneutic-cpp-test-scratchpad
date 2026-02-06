set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/aggregator.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(PROTO_SRCS ${GENERATED_DIR}/aggregator.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/aggregator.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/aggregator.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/aggregator.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND $<TARGET_FILE:protobuf::protoc>
  ARGS --grpc_out ${GENERATED_DIR}
       --cpp_out ${GENERATED_DIR}
       -I ${CMAKE_CURRENT_SOURCE_DIR}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating aggregator proto"
)

add_library(aggregator_proto STATIC
  ${PROTO_SRCS}
  ${PROTO_HDRS}
  ${GRPC_SRCS}
  ${GRPC_HDRS}
)

target_include_directories(aggregator_proto PUBLIC ${GENERATED_DIR})
target_link_libraries(aggregator_proto PUBLIC protobuf::libprotobuf grpc++ project_options project_warnings)
