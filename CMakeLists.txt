cmake_minimum_required(VERSION 3.20...4.0)

project(server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(src/net)

# enable tests
option(SERVER_BUILD_TESTS "Build unit tests" ON)
if (SERVER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(src/tests)
endif()
