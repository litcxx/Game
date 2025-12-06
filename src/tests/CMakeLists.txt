find_package(GTest QUIET)

if (NOT GTest_FOUND)
  message(STATUS "GTest not found — fetching from GitHub...")

  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)
else()
  message(STATUS "Using system-installed GTest")
endif()

set(SOURCES
  main.cpp
  net/protocol/net_packet_tests.cpp
  net/session/packet_handler_tests.cpp
  shared/ts_queue_tests.cpp
  game/player/player_tests.cpp
  game/world/world_tests.cpp
  game/physics/collision_tests.cpp
  game/physics/swept_math_tests.cpp
)

add_executable(unit_tests 
  ${SOURCES}
)

target_link_libraries(unit_tests PRIVATE
  GTest::gtest
  GTest::gtest_main
  GTest::gmock
  net
  game
)

target_include_directories(unit_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src/net
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/tests
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
