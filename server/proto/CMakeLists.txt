# Builds the shared protobuf schema (../../protocol/*.proto) into the `proto`
# library. protoc + libprotobuf are taken from the system install (config mode),
# which also provides the protobuf_generate() helper and the protobuf::protoc /
# protobuf::libprotobuf imported targets.
find_package(Protobuf CONFIG REQUIRED)

# The .proto sources live at the repository root, outside the server tree.
# CMAKE_SOURCE_DIR points at server/, so its parent is the repo root.
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/../protocol)

add_library(proto STATIC
    ${PROTO_DIR}/game/v1/protocol.proto
)

# libprotobuf (and its abseil/utf8_range deps) is a PUBLIC requirement: anything
# using the generated headers needs it at compile and link time.
#
# NOTE: intentionally NOT linking project_options here. That interface carries
# -fsanitize=address,undefined, and instrumenting the generated code while the
# system libprotobuf runtime is *not* instrumented produces an ABI/layout
# mismatch that crashes at parse time. The generated code must match the
# prebuilt libprotobuf; linking this (uninstrumented) static lib into a
# sanitized executable is supported by ASan/UBSan.
target_link_libraries(proto PUBLIC protobuf::libprotobuf)

# Generated *.pb.h land in the build tree next to this CMakeLists. Mark the dir
# SYSTEM so our strict warnings (-Werror ...) never fire on generated code.
target_include_directories(proto SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Generate C++ from the .proto sources already attached to the target above.
protobuf_generate(
    TARGET proto
    LANGUAGE cpp
    IMPORT_DIRS ${PROTO_DIR}
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)
