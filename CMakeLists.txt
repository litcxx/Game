cmake_minimum_required(VERSION 3.20...4.0)

project(server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src/net)

# enable tests
option(MMO_SERVER_BUILD_TESTS "Build unit tests" OFF)
if (SERVER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(src/tests)
endif()
