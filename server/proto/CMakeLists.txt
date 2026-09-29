# Builds the shared protobuf schema (../../protocol/*.proto) into the `proto`
# library. protoc + libprotobuf (with abseil) come from protobuf's own CMake
# package, version 36 — the one CI builds and tests with, installed by
# server/scripts/install-protobuf.sh. Nothing older: distro packages such as
# Ubuntu 24.04's 3.21 ship no CMake package, and newer headers differ
# ([[nodiscard]] parsing, ASan container annotations) — only one version is tested.
# (protobuf's version check wants its C++ runtime major, 7, not 36: so it is
# compared by hand.)
set(PROTOBUF_MIN_VERSION 36)
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND OR Protobuf_VERSION VERSION_LESS PROTOBUF_MIN_VERSION)
    if(Protobuf_FOUND)
        set(found "${Protobuf_VERSION} (${Protobuf_DIR})")
    else()
        set(found "none")
    endif()
    message(FATAL_ERROR
        "protobuf >= ${PROTOBUF_MIN_VERSION} with its CMake package is required; found: ${found}.\n"
        "Install it with server/scripts/install-protobuf.sh [prefix] and, for a prefix other "
        "than /usr/local, pass -DCMAKE_PREFIX_PATH=<prefix>.")
endif()
message(STATUS "protobuf ${Protobuf_VERSION}: ${Protobuf_DIR}")

# The .proto sources live at the repository root, outside the server tree.
# CMAKE_SOURCE_DIR points at server/, so its parent is the repo root.
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/../protocol)
set(PROTO_FILES ${PROTO_DIR}/game/v1/protocol.proto)

# <build>/proto/<path under protocol/>.pb.{h,cc}, e.g. game/v1/protocol.pb.h.
set(PROTO_SRCS)
foreach(proto ${PROTO_FILES})
    file(RELATIVE_PATH rel ${PROTO_DIR} ${proto})
    string(REGEX REPLACE "\\.proto$" "" stem ${rel})
    set(cc ${CMAKE_CURRENT_BINARY_DIR}/${stem}.pb.cc)
    set(h ${CMAKE_CURRENT_BINARY_DIR}/${stem}.pb.h)
    add_custom_command(
        OUTPUT ${cc} ${h}
        COMMAND protobuf::protoc --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I${PROTO_DIR} ${proto}
        DEPENDS ${proto} protobuf::protoc
        COMMENT "protoc ${rel}"
        VERBATIM)
    list(APPEND PROTO_SRCS ${cc} ${h})
endforeach()

add_library(proto STATIC ${PROTO_SRCS})

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
