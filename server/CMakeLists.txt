cmake_minimum_required(VERSION 3.20...4.0)

project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Default to a Debug build (warnings + sanitizers) on single-config generators
# when the user did not pick a build type, so the quality flags actually apply.
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# Shared dependency used by both net and cryptography.
find_package(OpenSSL REQUIRED)

# --- PROJECT OPTIONS (standard, sanitizers, optimization) ---
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)

set(SANITIZER "address" CACHE STRING "Sanitizer: address | thread | none(Valgrind)")
set_property(CACHE SANITIZER PROPERTY STRINGS none address thread)

if(SANITIZER STREQUAL "address")
    set(san_flags -fsanitize=address,undefined)
elseif(SANITIZER STREQUAL "thread")
    set(san_flags -fsanitize=thread)
elseif(SANITIZER STREQUAL "none")
    set(san_flags "")
else()
    message(FATAL_ERROR "Unknown SANITIZER='${SANITIZER}' (none|address|thread)")
endif()

set(debug_flags -g -fno-omit-frame-pointer ${san_flags})

target_compile_options(project_options INTERFACE
    "$<$<CONFIG:Debug>:${debug_flags}>"
    "$<$<CONFIG:Release>:-O3;-g;-fno-omit-frame-pointer>"
)
target_link_options(project_options INTERFACE "$<$<CONFIG:Debug>:${san_flags}>")

# --- PROJECT WARNINGS ---
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion
    -Wold-style-cast
    "$<$<BOOL:${WARNINGS_AS_ERRORS}>:-Werror>"
)

# --- SUBDIRECTORIES ---
add_subdirectory(third_party)
add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(src/cryptography)
add_subdirectory(src/db)
add_subdirectory(src/game)
add_subdirectory(src/net)
add_subdirectory(src/shared)

# --- TESTS ---
option(USE_TESTS "Build tests" ON)
if (USE_TESTS)
    enable_testing()
    add_subdirectory(test/unit)
    add_subdirectory(test/integration)
endif()


# --- CLANG-FORMAT ---
option(USE_CLANG_FORMAT "Build clang-format" ON)
if (USE_CLANG_FORMAT)
    find_program(CLANG_FORMAT NAMES clang-format-21)
    if (CLANG_FORMAT)
        file(GLOB_RECURSE CLANG_FORMAT_SRC CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
        )

        add_custom_target(clang-format-fix
            COMMAND ${CLANG_FORMAT} -style=file -i ${CLANG_FORMAT_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Formatting ${PROJECT_NAME} with ${CLANG_FORMAT} ..."
        )

        add_custom_target(clang-format-check
            COMMAND run-clang-format.py --clang-format-executable=${CLANG_FORMAT} --style=file ${CLANG_FORMAT_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Checking format ${PROJECT_NAME} with ${CLANG_FORMAT} ..."
        )
    else()
        message(WARNING "clang-format-21 is not found: ${CLANG_FORMAT}")
    endif()
endif()

# --- CLANG-TIDY ---
option(USE_CLANG_TIDY "Build clang-tidy" ON)
if (USE_CLANG_TIDY)
    find_program(CLANG_TIDY NAMES clang-tidy-21)

    if(CLANG_TIDY)
        # Only translation units (.cpp) are analyzed; headers are checked via the
        # .cpp that include them (see HeaderFilterRegex in .clang-tidy). Passing
        # headers directly makes clang-tidy try to compile them standalone.
        file(GLOB_RECURSE CLANG_TIDY_SRC CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        )

        add_custom_target(clang-tidy
            COMMAND ${CLANG_TIDY}
                    -p ${CMAKE_BINARY_DIR}
                    -config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
                    ${CLANG_TIDY_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy ..."
        )
    else()
        message(WARNING "clang-tidy-21 is not found: ${CLANG_TIDY}")
    endif()
endif()
